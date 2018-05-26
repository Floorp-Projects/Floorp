/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {WindowWrapper, NotifierEvent};
use blob;
use euclid::{TypedRect, TypedSize2D, TypedPoint2D};
use std::sync::Arc;
use std::sync::atomic::{AtomicIsize, Ordering};
use std::sync::mpsc::Receiver;
use webrender::api::*;
use wrench::Wrench;

pub struct RawtestHarness<'a> {
    wrench: &'a mut Wrench,
    rx: &'a Receiver<NotifierEvent>,
    window: &'a mut WindowWrapper,
}

fn point<T: Copy, U>(x: T, y: T) -> TypedPoint2D<T, U> {
    TypedPoint2D::new(x, y)
}

fn size<T: Copy, U>(x: T, y: T) -> TypedSize2D<T, U> {
    TypedSize2D::new(x, y)
}

fn rect<T: Copy, U>(x: T, y: T, width: T, height: T) -> TypedRect<T, U> {
    TypedRect::new(point(x, y), size(width, height))
}

impl<'a> RawtestHarness<'a> {
    pub fn new(wrench: &'a mut Wrench,
               window: &'a mut WindowWrapper,
               rx: &'a Receiver<NotifierEvent>) -> Self {
        RawtestHarness {
            wrench,
            rx,
            window,
        }
    }

    pub fn run(mut self) {
        self.test_hit_testing();
        self.test_retained_blob_images_test();
        self.test_blob_update_test();
        self.test_blob_update_epoch_test();
        self.test_tile_decomposition();
        self.test_very_large_blob();
        self.test_offscreen_blob();
        self.test_save_restore();
        self.test_capture();
        self.test_zero_height_window();
    }

    fn render_and_get_pixels(&mut self, window_rect: DeviceUintRect) -> Vec<u8> {
        self.rx.recv().unwrap();
        self.wrench.render();
        self.wrench.renderer.read_pixels_rgba8(window_rect)
    }

    fn submit_dl(
        &mut self,
        epoch: &mut Epoch,
        layout_size: LayoutSize,
        builder: DisplayListBuilder,
        resources: &[ResourceUpdate]
    ) {
        let mut txn = Transaction::new();
        let root_background_color = Some(ColorF::new(1.0, 1.0, 1.0, 1.0));
        txn.use_scene_builder_thread();
        if !resources.is_empty() {
            txn.resource_updates = resources.to_vec();
        }
        txn.set_display_list(
            *epoch,
            root_background_color,
            layout_size,
            builder.finalize(),
            false,
        );
        epoch.0 += 1;

        txn.generate_frame();
        self.wrench.api.send_transaction(self.wrench.document_id, txn);
    }


    fn test_tile_decomposition(&mut self) {
        println!("\ttile decomposition...");
        // This exposes a crash in tile decomposition
        let layout_size = LayoutSize::new(800., 800.);
        let mut txn = Transaction::new();

        let blob_img = self.wrench.api.generate_image_key();
        txn.add_image(
            blob_img,
            ImageDescriptor::new(151, 56, ImageFormat::BGRA8, true, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
            Some(128),
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        let info = LayoutPrimitiveInfo::new(rect(448.899994, 74.0, 151.000031, 56.));

        // setup some malicious image size parameters
        builder.push_image(
            &info,
            size(151., 56.0),
            size(151.0, 56.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        let mut epoch = Epoch(0);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        self.rx.recv().unwrap();
        self.wrench.render();

        // Leaving a tiled blob image in the resource cache
        // confuses the `test_capture`. TODO: remove this
        txn = Transaction::new();
        txn.delete_image(blob_img);
        self.wrench.api.update_resources(txn.resource_updates);
    }

    fn test_very_large_blob(&mut self) {
        println!("\tvery large blob...");

        assert_eq!(self.wrench.device_pixel_ratio, 1.);

        let window_size = self.window.get_inner_size();

        let test_size = DeviceUintSize::new(800, 800);

        let window_rect = DeviceUintRect::new(
            DeviceUintPoint::new(0, window_size.height - test_size.height),
            test_size,
        );

        // This exposes a crash in tile decomposition
        let layout_size = LayoutSize::new(800., 800.);
        let mut txn = Transaction::new();

        let blob_img = self.wrench.api.generate_image_key();
        txn.add_image(
            blob_img,
            ImageDescriptor::new(1510, 111256, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
            Some(31),
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        let info = LayoutPrimitiveInfo::new(rect(0., -9600.0, 1510.000031, 111256.));

        let image_size = size(1510., 111256.);

        let clip_id = builder.define_clip(rect(40., 41., 200., 201.), vec![], None);

        builder.push_clip_id(clip_id);
        // setup some malicious image size parameters
        builder.push_image(
            &info,
            image_size * 2.,
            image_size,
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );
        builder.pop_clip_id();

        let mut epoch = Epoch(0);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        let called = Arc::new(AtomicIsize::new(0));
        let called_inner = Arc::clone(&called);

        self.wrench.callbacks.lock().unwrap().request = Box::new(move |_| {
            called_inner.fetch_add(1, Ordering::SeqCst);
        });

        let pixels = self.render_and_get_pixels(window_rect);

        // make sure we didn't request too many blobs
        assert_eq!(called.load(Ordering::SeqCst), 16);

        // make sure things are in the right spot
        assert!(
            pixels[(148 +
                (window_rect.size.height as usize - 148) *
                    window_rect.size.width as usize) * 4] == 255 &&
                pixels[(148 +
                    (window_rect.size.height as usize - 148) *
                        window_rect.size.width as usize) * 4 + 1] == 255 &&
                pixels[(148 +
                    (window_rect.size.height as usize - 148) *
                        window_rect.size.width as usize) * 4 + 2] == 255 &&
                pixels[(148 +
                    (window_rect.size.height as usize - 148) *
                        window_rect.size.width as usize) * 4 + 3] == 255
        );
        assert!(
            pixels[(132 +
                (window_rect.size.height as usize - 148) *
                    window_rect.size.width as usize) * 4] == 50 &&
                pixels[(132 +
                    (window_rect.size.height as usize - 148) *
                        window_rect.size.width as usize) * 4 + 1] == 50 &&
                pixels[(132 +
                    (window_rect.size.height as usize - 148) *
                        window_rect.size.width as usize) * 4 + 2] == 150 &&
                pixels[(132 +
                    (window_rect.size.height as usize - 148) *
                        window_rect.size.width as usize) * 4 + 3] == 255
        );

        // Leaving a tiled blob image in the resource cache
        // confuses the `test_capture`. TODO: remove this
        txn = Transaction::new();
        txn.delete_image(blob_img);
        self.wrench.api.update_resources(txn.resource_updates);
    }

    fn test_offscreen_blob(&mut self) {
        println!("\toffscreen blob update.");

        assert_eq!(self.wrench.device_pixel_ratio, 1.);

        let window_size = self.window.get_inner_size();

        let test_size = DeviceUintSize::new(800, 800);

        let window_rect = DeviceUintRect::new(
            DeviceUintPoint::new(0, window_size.height - test_size.height),
            test_size,
        );

        // This exposes a crash in tile decomposition
        let mut txn = Transaction::new();
        let layout_size = LayoutSize::new(800., 800.);

        let blob_img = self.wrench.api.generate_image_key();
        txn.add_image(
            blob_img,
            ImageDescriptor::new(1510, 1510, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
            None,
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        let info = LayoutPrimitiveInfo::new(rect(0., 0.0, 1510., 1510.));

        let image_size = size(1510., 1510.);

        // setup some malicious image size parameters
        builder.push_image(
            &info,
            image_size,
            image_size,
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        let mut epoch = Epoch(0);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        let original_pixels = self.render_and_get_pixels(window_rect);

        let mut epoch = Epoch(1);

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        let info = LayoutPrimitiveInfo::new(rect(-10000., 0.0, 1510., 1510.));

        let image_size = size(1510., 1510.);

        // setup some malicious image size parameters
        builder.push_image(
            &info,
            image_size,
            image_size,
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        self.submit_dl(&mut epoch, layout_size, builder, &[]);

        let _offscreen_pixels = self.render_and_get_pixels(window_rect);

        let mut txn = Transaction::new();

        txn.update_image(
            blob_img,
            ImageDescriptor::new(1510, 1510, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
            Some(rect(10, 10, 100, 100)),
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        let info = LayoutPrimitiveInfo::new(rect(0., 0.0, 1510., 1510.));

        let image_size = size(1510., 1510.);

        // setup some malicious image size parameters
        builder.push_image(
            &info,
            image_size,
            image_size,
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        let mut epoch = Epoch(2);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        let pixels = self.render_and_get_pixels(window_rect);

        assert!(pixels == original_pixels);

        // Leaving a tiled blob image in the resource cache
        // confuses the `test_capture`. TODO: remove this
        txn = Transaction::new();
        txn.delete_image(blob_img);
        self.wrench.api.update_resources(txn.resource_updates);
    }

    fn test_retained_blob_images_test(&mut self) {
        println!("\tretained blob images test...");
        let blob_img;
        let window_size = self.window.get_inner_size();

        let test_size = DeviceUintSize::new(400, 400);

        let window_rect = DeviceUintRect::new(
            DeviceUintPoint::new(0, window_size.height - test_size.height),
            test_size,
        );
        let layout_size = LayoutSize::new(400., 400.);
        let mut txn = Transaction::new();
        {
            let api = &self.wrench.api;

            blob_img = api.generate_image_key();
            txn.add_image(
                blob_img,
                ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
                ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
                None,
            );
        }

        // draw the blob the first time
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(rect(0.0, 60.0, 200.0, 200.0));

        builder.push_image(
            &info,
            size(200.0, 200.0),
            size(0.0, 0.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        let mut epoch = Epoch(0);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        let called = Arc::new(AtomicIsize::new(0));
        let called_inner = Arc::clone(&called);

        self.wrench.callbacks.lock().unwrap().request = Box::new(move |_| {
            called_inner.fetch_add(1, Ordering::SeqCst);
        });

        let pixels_first = self.render_and_get_pixels(window_rect);

        assert!(called.load(Ordering::SeqCst) == 1);

        // draw the blob image a second time at a different location

        // make a new display list that refers to the first image
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(rect(1.0, 60.0, 200.0, 200.0));
        builder.push_image(
            &info,
            size(200.0, 200.0),
            size(0.0, 0.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        txn.resource_updates.clear();

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        let pixels_second = self.render_and_get_pixels(window_rect);

        // make sure we only requested once
        assert!(called.load(Ordering::SeqCst) == 1);

        // use png;
        // png::save_flipped("out1.png", &pixels_first, window_rect.size);
        // png::save_flipped("out2.png", &pixels_second, window_rect.size);

        assert!(pixels_first != pixels_second);
    }

    fn test_blob_update_epoch_test(&mut self) {
        println!("\tblob update epoch test...");
        let (blob_img, blob_img2);
        let window_size = self.window.get_inner_size();

        let test_size = DeviceUintSize::new(400, 400);

        let window_rect = DeviceUintRect::new(
            point(0, window_size.height - test_size.height),
            test_size,
        );
        let layout_size = LayoutSize::new(400., 400.);
        let mut txn = Transaction::new();
        let (blob_img, blob_img2) = {
            let api = &self.wrench.api;

            blob_img = api.generate_image_key();
            txn.add_image(
                blob_img,
                ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
                ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
                None,
            );
            blob_img2 = api.generate_image_key();
            txn.add_image(
                blob_img2,
                ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
                ImageData::new_blob_image(blob::serialize_blob(ColorU::new(80, 50, 150, 255))),
                None,
            );
            (blob_img, blob_img2)
        };

        // setup some counters to count how many times each image is requested
        let img1_requested = Arc::new(AtomicIsize::new(0));
        let img1_requested_inner = Arc::clone(&img1_requested);
        let img2_requested = Arc::new(AtomicIsize::new(0));
        let img2_requested_inner = Arc::clone(&img2_requested);

        // track the number of times that the second image has been requested
        self.wrench.callbacks.lock().unwrap().request = Box::new(move |&desc| {
            if desc.key == blob_img {
                img1_requested_inner.fetch_add(1, Ordering::SeqCst);
            }
            if desc.key == blob_img2 {
                img2_requested_inner.fetch_add(1, Ordering::SeqCst);
            }
        });

        // create two blob images and draw them
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(rect(0.0, 60.0, 200.0, 200.0));
        let info2 = LayoutPrimitiveInfo::new(rect(200.0, 60.0, 200.0, 200.0));
        let push_images = |builder: &mut DisplayListBuilder| {
            builder.push_image(
                &info,
                size(200.0, 200.0),
                size(0.0, 0.0),
                ImageRendering::Auto,
                AlphaType::PremultipliedAlpha,
                blob_img,
            );
            builder.push_image(
                &info2,
                size(200.0, 200.0),
                size(0.0, 0.0),
                ImageRendering::Auto,
                AlphaType::PremultipliedAlpha,
                blob_img2,
            );
        };

        push_images(&mut builder);

        let mut epoch = Epoch(0);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);
        let _pixels_first = self.render_and_get_pixels(window_rect);


        // update and redraw both images
        let mut txn = Transaction::new();
        txn.update_image(
            blob_img,
            ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
            Some(rect(100, 100, 100, 100)),
        );
        txn.update_image(
            blob_img2,
            ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(59, 50, 150, 255))),
            Some(rect(100, 100, 100, 100)),
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        push_images(&mut builder);
        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);
        let _pixels_second = self.render_and_get_pixels(window_rect);


        // only update the first image
        let mut txn = Transaction::new();
        txn.update_image(
            blob_img,
            ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 150, 150, 255))),
            Some(rect(200, 200, 100, 100)),
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        push_images(&mut builder);
        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);
        let _pixels_third = self.render_and_get_pixels(window_rect);

        // the first image should be requested 3 times
        assert_eq!(img1_requested.load(Ordering::SeqCst), 3);
        // the second image should've been requested twice
        assert_eq!(img2_requested.load(Ordering::SeqCst), 2);
    }

    fn test_blob_update_test(&mut self) {
        println!("\tblob update test...");
        let window_size = self.window.get_inner_size();

        let test_size = DeviceUintSize::new(400, 400);

        let window_rect = DeviceUintRect::new(
            point(0, window_size.height - test_size.height),
            test_size,
        );
        let layout_size = LayoutSize::new(400., 400.);
        let mut txn = Transaction::new();

        let blob_img = {
            let img = self.wrench.api.generate_image_key();
            txn.add_image(
                img,
                ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
                ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
                None,
            );
            img
        };

        // draw the blobs the first time
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(rect(0.0, 60.0, 200.0, 200.0));

        builder.push_image(
            &info,
            size(200.0, 200.0),
            size(0.0, 0.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        let mut epoch = Epoch(0);

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);
        let pixels_first = self.render_and_get_pixels(window_rect);

        // draw the blob image a second time after updating it with the same color
        let mut txn = Transaction::new();
        txn.update_image(
            blob_img,
            ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 50, 150, 255))),
            Some(rect(100, 100, 100, 100)),
        );

        // make a new display list that refers to the first image
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(rect(0.0, 60.0, 200.0, 200.0));
        builder.push_image(
            &info,
            size(200.0, 200.0),
            size(0.0, 0.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);
        let pixels_second = self.render_and_get_pixels(window_rect);

        // draw the blob image a third time after updating it with a different color
        let mut txn = Transaction::new();
        txn.update_image(
            blob_img,
            ImageDescriptor::new(500, 500, ImageFormat::BGRA8, false, false),
            ImageData::new_blob_image(blob::serialize_blob(ColorU::new(50, 150, 150, 255))),
            Some(rect(200, 200, 100, 100)),
        );

        // make a new display list that refers to the first image
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(rect(0.0, 60.0, 200.0, 200.0));
        builder.push_image(
            &info,
            size(200.0, 200.0),
            size(0.0, 0.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            blob_img,
        );

        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);
        let pixels_third = self.render_and_get_pixels(window_rect);

        assert!(pixels_first == pixels_second);
        assert!(pixels_first != pixels_third);
    }

    // Ensures that content doing a save-restore produces the same results as not
    fn test_save_restore(&mut self) {
        println!("\tsave/restore...");
        let window_size = self.window.get_inner_size();

        let test_size = DeviceUintSize::new(400, 400);

        let window_rect = DeviceUintRect::new(
            DeviceUintPoint::new(0, window_size.height - test_size.height),
            test_size,
        );
        let layout_size = LayoutSize::new(400., 400.);

        let mut do_test = |should_try_and_fail| {
            let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

            let clip = builder.define_clip(
                rect(110., 120., 200., 200.),
                None::<ComplexClipRegion>,
                None
            );
            builder.push_clip_id(clip);
            builder.push_rect(&PrimitiveInfo::new(rect(100., 100., 100., 100.)),
                              ColorF::new(0.0, 0.0, 1.0, 1.0));

            if should_try_and_fail {
                builder.save();
                let clip = builder.define_clip(
                    rect(80., 80., 90., 90.),
                    None::<ComplexClipRegion>,
                    None
                );
                builder.push_clip_id(clip);
                builder.push_rect(&PrimitiveInfo::new(rect(110., 110., 50., 50.)),
                                  ColorF::new(0.0, 1.0, 0.0, 1.0));
                builder.push_shadow(&PrimitiveInfo::new(rect(100., 100., 100., 100.)),
                                    Shadow {
                                        offset: LayoutVector2D::new(1.0, 1.0),
                                        blur_radius: 1.0,
                                        color: ColorF::new(0.0, 0.0, 0.0, 1.0),
                                    });
                builder.push_line(&PrimitiveInfo::new(rect(110., 110., 50., 2.)),
                                  0.0, LineOrientation::Horizontal,
                                  &ColorF::new(0.0, 0.0, 0.0, 1.0), LineStyle::Solid);
                builder.restore();
            }

            {
                builder.save();
                let clip = builder.define_clip(
                    rect(80., 80., 100., 100.),
                    None::<ComplexClipRegion>,
                    None
                );
                builder.push_clip_id(clip);
                builder.push_rect(&PrimitiveInfo::new(rect(150., 150., 100., 100.)),
                                  ColorF::new(0.0, 0.0, 1.0, 1.0));

                builder.pop_clip_id();
                builder.clear_save();
            }

            builder.pop_clip_id();

            let txn = Transaction::new();

            self.submit_dl(&mut Epoch(0), layout_size, builder, &txn.resource_updates);

            self.render_and_get_pixels(window_rect)
        };

        let first = do_test(false);
        let second = do_test(true);

        assert_eq!(first, second);
    }

    fn test_capture(&mut self) {
        println!("\tcapture...");
        let path = "../captures/test";
        let layout_size = LayoutSize::new(400., 400.);
        let dim = self.window.get_inner_size();
        let window_rect = DeviceUintRect::new(
            point(0, dim.height - layout_size.height as u32),
            size(layout_size.width as u32, layout_size.height as u32),
        );

        // 1. render some scene

        let mut txn = Transaction::new();
        let image = self.wrench.api.generate_image_key();
        txn.add_image(
            image,
            ImageDescriptor::new(1, 1, ImageFormat::BGRA8, true, false),
            ImageData::new(vec![0xFF, 0, 0, 0xFF]),
            None,
        );

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        builder.push_image(
            &LayoutPrimitiveInfo::new(rect(300.0, 70.0, 150.0, 50.0)),
            size(150.0, 50.0),
            size(0.0, 0.0),
            ImageRendering::Auto,
            AlphaType::PremultipliedAlpha,
            image,
        );

        let mut txn = Transaction::new();

        txn.set_display_list(
            Epoch(0),
            Some(ColorF::new(1.0, 1.0, 1.0, 1.0)),
            layout_size,
            builder.finalize(),
            false,
        );
        txn.generate_frame();

        self.wrench.api.send_transaction(self.wrench.document_id, txn);

        let pixels0 = self.render_and_get_pixels(window_rect);

        // 2. capture it
        self.wrench.api.save_capture(path.into(), CaptureBits::all());

        // 3. set a different scene

        builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        let mut txn = Transaction::new();
        txn.set_display_list(
            Epoch(1),
            Some(ColorF::new(1.0, 0.0, 0.0, 1.0)),
            layout_size,
            builder.finalize(),
            false,
        );
        self.wrench.api.send_transaction(self.wrench.document_id, txn);

        // 4. load the first one

        let mut documents = self.wrench.api.load_capture(path.into());
        let captured = documents.swap_remove(0);

        // 5. render the built frame and compare
        let pixels1 = self.render_and_get_pixels(window_rect);
        assert!(pixels0 == pixels1);

        // 6. rebuild the scene and compare again
        let mut txn = Transaction::new();
        txn.set_root_pipeline(captured.root_pipeline_id.unwrap());
        txn.generate_frame();
        self.wrench.api.send_transaction(captured.document_id, txn);
        let pixels2 = self.render_and_get_pixels(window_rect);
        assert!(pixels0 == pixels2);
    }

    fn test_zero_height_window(&mut self) {
        println!("\tzero height test...");

        let layout_size = LayoutSize::new(120.0, 0.0);
        let window_size = DeviceUintSize::new(layout_size.width as u32, layout_size.height as u32);
        let doc_id = self.wrench.api.add_document(window_size, 1);

        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);
        let info = LayoutPrimitiveInfo::new(LayoutRect::new(LayoutPoint::zero(),
                                                            LayoutSize::new(100.0, 100.0)));
        builder.push_rect(&info, ColorF::new(0.0, 1.0, 0.0, 1.0));

        let mut txn = Transaction::new();
        txn.set_root_pipeline(self.wrench.root_pipeline_id);
        txn.set_display_list(
            Epoch(1),
            Some(ColorF::new(1.0, 0.0, 0.0, 1.0)),
            layout_size,
            builder.finalize(),
            false,
        );
        txn.generate_frame();
        self.wrench.api.send_transaction(doc_id, txn);

        // Ensure we get a notification from rendering the above, even though
        // there are zero visible pixels
        assert!(self.rx.recv().unwrap() == NotifierEvent::WakeUp);
    }


    fn test_hit_testing(&mut self) {
        println!("\thit testing test...");

        let layout_size = LayoutSize::new(400., 400.);
        let mut builder = DisplayListBuilder::new(self.wrench.root_pipeline_id, layout_size);

        // Add a rectangle that covers the entire scene.
        let mut info = LayoutPrimitiveInfo::new(LayoutRect::new(LayoutPoint::zero(), layout_size));
        info.tag = Some((0, 1));
        builder.push_rect(&info, ColorF::new(1.0, 1.0, 1.0, 1.0));


        // Add a simple 100x100 rectangle at 100,0.
        let mut info = LayoutPrimitiveInfo::new(LayoutRect::new(
            LayoutPoint::new(100., 0.),
            LayoutSize::new(100., 100.)
        ));
        info.tag = Some((0, 2));
        builder.push_rect(&info, ColorF::new(1.0, 1.0, 1.0, 1.0));

        let make_rounded_complex_clip = |rect: &LayoutRect, radius: f32| -> ComplexClipRegion {
            ComplexClipRegion::new(
                *rect,
                BorderRadius::uniform_size(LayoutSize::new(radius, radius)),
                ClipMode::Clip
            )
        };


        // Add a rectangle that is clipped by a rounded rect clip item.
        let rect = LayoutRect::new(LayoutPoint::new(100., 100.), LayoutSize::new(100., 100.));
        let clip_id = builder.define_clip(rect, vec![make_rounded_complex_clip(&rect, 20.)], None);
        builder.push_clip_id(clip_id);
        let mut info = LayoutPrimitiveInfo::new(rect);
        info.tag = Some((0, 4));
        builder.push_rect(&info, ColorF::new(1.0, 1.0, 1.0, 1.0));
        builder.pop_clip_id();


        // Add a rectangle that is clipped by a ClipChain containing a rounded rect.
        let rect = LayoutRect::new(LayoutPoint::new(200., 100.), LayoutSize::new(100., 100.));
        let clip_id = builder.define_clip(rect, vec![make_rounded_complex_clip(&rect, 20.)], None);
        let clip_chain_id = builder.define_clip_chain(None, vec![clip_id]);
        builder.push_clip_and_scroll_info(ClipAndScrollInfo::new(
            ClipId::root_scroll_node(self.wrench.root_pipeline_id),
            ClipId::ClipChain(clip_chain_id),
        ));
        let mut info = LayoutPrimitiveInfo::new(rect);
        info.tag = Some((0, 5));
        builder.push_rect(&info, ColorF::new(1.0, 1.0, 1.0, 1.0));
        builder.pop_clip_id();


        let mut epoch = Epoch(0);
        let txn = Transaction::new();
        self.submit_dl(&mut epoch, layout_size, builder, &txn.resource_updates);

        // We render to ensure that the hit tester is up to date with the current scene.
        self.rx.recv().unwrap();
        self.wrench.render();

        let hit_test = |point: WorldPoint| -> HitTestResult {
            self.wrench.api.hit_test(
                self.wrench.document_id,
                None,
                point,
                HitTestFlags::FIND_ALL,
            )
        };

        let assert_hit_test = |point: WorldPoint, tags: Vec<ItemTag>| {
            let result = hit_test(point);
            assert_eq!(result.items.len(), tags.len());

            for (hit_test_item, item_b) in result.items.iter().zip(tags.iter()) {
                assert_eq!(hit_test_item.tag, *item_b);
            }
        };

        // We should not have any hits outside the boundaries of the scene.
        assert_hit_test(WorldPoint::new(-10., -10.), Vec::new());
        assert_hit_test(WorldPoint::new(-10., 10.), Vec::new());
        assert_hit_test(WorldPoint::new(450., 450.), Vec::new());
        assert_hit_test(WorldPoint::new(100., 450.), Vec::new());

        // The top left corner of the scene should only contain the background.
        assert_hit_test(WorldPoint::new(50., 50.), vec![(0, 1)]);

        // The middle of the normal rectangle should be hit.
        assert_hit_test(WorldPoint::new(150., 50.), vec![(0, 2), (0, 1)]);

        let test_rounded_rectangle = |point: WorldPoint, size: WorldSize, tag: ItemTag| {
            // The cut out corners of the rounded rectangle should not be hit.
            let top_left = point + WorldVector2D::new(5., 5.);
            let bottom_right = point + size.to_vector() - WorldVector2D::new(5., 5.);

            assert_hit_test(
                WorldPoint::new(point.x + (size.width / 2.), point.y + (size.height / 2.)),
                vec![tag, (0, 1)]
            );

            assert_hit_test(top_left, vec![(0, 1)]);
            assert_hit_test(WorldPoint::new(bottom_right.x, top_left.y), vec![(0, 1)]);
            assert_hit_test(WorldPoint::new(top_left.x, bottom_right.y), vec![(0, 1)]);
            assert_hit_test(bottom_right, vec![(0, 1)]);
        };

        test_rounded_rectangle(WorldPoint::new(100., 100.), WorldSize::new(100., 100.), (0, 4));
        test_rounded_rectangle(WorldPoint::new(200., 100.), WorldSize::new(100., 100.), (0, 5));
    }

}
