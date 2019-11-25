/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

    An example of how to implement the Compositor trait that
    allows picture caching surfaces to be composited by the operating
    system.

    The current example supports DirectComposite on Windows only.

 */

use euclid::Angle;
use gleam::gl;
use std::ffi::CString;
use std::sync::mpsc;
use webrender::api::*;
use webrender::api::units::*;
#[cfg(target_os = "windows")]
use compositor_windows as compositor;

// A very hacky integration with DirectComposite. It proxies calls from the compositor
// interface to a simple C99 library which does the DirectComposition / D3D11 / ANGLE
// interfacing. This is a very unsafe impl due to the way the window pointer is passed
// around!
struct DirectCompositeInterface {
    window: *mut compositor::Window,
}

impl DirectCompositeInterface {
    fn new(window: *mut compositor::Window) -> Self {
        DirectCompositeInterface {
            window,
        }
    }
}

impl webrender::Compositor for DirectCompositeInterface {
    fn create_surface(
        &mut self,
        id: webrender::NativeSurfaceId,
        size: DeviceIntSize,
        is_opaque: bool,
    ) {
        compositor::create_surface(
            self.window,
            id.0,
            size.width,
            size.height,
            is_opaque,
        );
    }

    fn destroy_surface(
        &mut self,
        id: webrender::NativeSurfaceId,
    ) {
        compositor::destroy_surface(self.window, id.0);
    }

    fn bind(
        &mut self,
        id: webrender::NativeSurfaceId,
        dirty_rect: DeviceIntRect,
    ) -> webrender::NativeSurfaceInfo {
        let (fbo_id, x, y) = compositor::bind_surface(
            self.window,
            id.0,
            dirty_rect.origin.x,
            dirty_rect.origin.y,
            dirty_rect.size.width,
            dirty_rect.size.height,
        );

        webrender::NativeSurfaceInfo {
            origin: DeviceIntPoint::new(x, y),
            fbo_id,
        }
    }

    fn unbind(&mut self) {
        compositor::unbind_surface(self.window);
    }

    fn begin_frame(&mut self) {
        compositor::begin_transaction(self.window);
    }

    fn add_surface(
        &mut self,
        id: webrender::NativeSurfaceId,
        position: DeviceIntPoint,
        clip_rect: DeviceIntRect,
    ) {
        compositor::add_surface(
            self.window,
            id.0,
            position.x,
            position.y,
            clip_rect.origin.x,
            clip_rect.origin.y,
            clip_rect.size.width,
            clip_rect.size.height,
        );
    }

    fn end_frame(&mut self) {
        compositor::end_transaction(self.window);
    }
}

// Simplisitic implementation of the WR notifier interface to know when a frame
// has been prepared and can be rendered.
struct Notifier {
    tx: mpsc::Sender<()>,
}

impl Notifier {
    fn new(tx: mpsc::Sender<()>) -> Self {
        Notifier {
            tx,
        }
    }
}

impl RenderNotifier for Notifier {
    fn clone(&self) -> Box<dyn RenderNotifier> {
        Box::new(Notifier {
            tx: self.tx.clone()
        })
    }

    fn wake_up(&self) {
    }

    fn new_frame_ready(&self,
                       _: DocumentId,
                       _scrolled: bool,
                       _composite_needed: bool,
                       _render_time: Option<u64>) {
        self.tx.send(()).ok();
    }
}

fn push_rotated_rect(
    builder: &mut DisplayListBuilder,
    rect: LayoutRect,
    color: ColorF,
    spatial_id: SpatialId,
    root_pipeline_id: PipelineId,
    angle: f32,
    time: f32,
) {
    let color = color.scale_rgb(time);
    let rotation = LayoutTransform::create_rotation(
        0.0,
        0.0,
        1.0,
        Angle::radians(2.0 * std::f32::consts::PI * angle),
    );
    let transform_origin = LayoutVector3D::new(
        rect.origin.x + rect.size.width * 0.5,
        rect.origin.y + rect.size.height * 0.5,
        0.0,
    );
    let transform = rotation
        .pre_translate(-transform_origin)
        .post_translate(transform_origin);
    let spatial_id = builder.push_reference_frame(
        LayoutPoint::zero(),
        spatial_id,
        TransformStyle::Flat,
        PropertyBinding::Value(transform),
        ReferenceFrameKind::Transform,
    );
    builder.push_rect(
        &CommonItemProperties::new(
            rect,
            SpaceAndClipInfo {
                spatial_id,
                clip_id: ClipId::root(root_pipeline_id),
            },
        ),
        color,
    );
}

fn build_display_list(
    builder: &mut DisplayListBuilder,
    root_pipeline_id: PipelineId,
    layout_size: LayoutSize,
    time: f32,
) {
    let fixed_space_info = SpaceAndClipInfo {
        spatial_id: SpatialId::root_scroll_node(root_pipeline_id),
        clip_id: ClipId::root(root_pipeline_id),
    };

    let scroll_space_info = builder.define_scroll_frame(
        &fixed_space_info,
        None,
        LayoutRect::new(LayoutPoint::zero(), layout_size),
        LayoutRect::new(LayoutPoint::zero(), layout_size),
        Vec::new(),
        None,
        ScrollSensitivity::Script,
        LayoutVector2D::zero(),
    );

    builder.push_rect(
        &CommonItemProperties::new(
            LayoutRect::new(LayoutPoint::zero(), layout_size).inflate(-10.0, -10.0),
            fixed_space_info,
        ),
        ColorF::new(0.8, 0.8, 0.8, 1.0),
    );

    push_rotated_rect(
        builder,
        LayoutRect::new(
            LayoutPoint::new(100.0, 100.0),
            LayoutSize::new(400.0, 400.0),
        ),
        ColorF::new(1.0, 0.0, 0.0, 1.0),
        scroll_space_info.spatial_id,
        root_pipeline_id,
        0.0,
        time,
    );

    push_rotated_rect(
        builder,
        LayoutRect::new(
            LayoutPoint::new(800.0, 100.0),
            LayoutSize::new(100.0, 600.0),
        ),
        ColorF::new(0.0, 1.0, 0.0, 1.0),
        fixed_space_info.spatial_id,
        root_pipeline_id,
        0.2,
        time,
    );

    push_rotated_rect(
        builder,
        LayoutRect::new(
            LayoutPoint::new(700.0, 200.0),
            LayoutSize::new(300.0, 300.0),
        ),
        ColorF::new(0.0, 0.0, 1.0, 1.0),
        scroll_space_info.spatial_id,
        root_pipeline_id,
        0.1,
        time,
    );
}

fn main() {
    // If true, use DirectComposition. If false, this will fall back to normal
    // WR compositing of picture caching tiles, which can be used to check
    // correctness of implementation.
    let enable_compositor = true;

    // Load GL, construct WR and the native compositor interface.
    let device_size = DeviceIntSize::new(1600, 800);
    let window = compositor::create_window(
        device_size.width,
        device_size.height,
        enable_compositor,
    );
    let debug_flags = DebugFlags::empty();
    let compositor_config = if enable_compositor {
        webrender::CompositorConfig::Native {
            max_update_rects: 1,
            compositor: Box::new(DirectCompositeInterface::new(window)),
        }
    } else {
        webrender::CompositorConfig::Draw {
            max_partial_present_rects: 0,
        }
    };
    let opts = webrender::RendererOptions {
        clear_color: Some(ColorF::new(1.0, 1.0, 1.0, 1.0)),
        debug_flags,
        enable_picture_caching: true,
        compositor_config,
        ..webrender::RendererOptions::default()
    };
    let (tx, rx) = mpsc::channel();
    let notifier = Box::new(Notifier::new(tx));
    let gl = unsafe {
        gl::GlesFns::load_with(
            |symbol| {
                let symbol = CString::new(symbol).unwrap();
                let ptr = compositor::get_proc_address(symbol.as_ptr());
                ptr
            }
        )
    };
    let (mut renderer, sender) = webrender::Renderer::new(
        gl.clone(),
        notifier,
        opts,
        None,
        device_size,
    ).unwrap();
    let api = sender.create_api();
    let document_id = api.add_document(device_size, 0);
    let device_pixel_ratio = 1.0;
    let mut current_epoch = Epoch(0);
    let root_pipeline_id = PipelineId(0, 0);

    // Kick off first transaction which will mean we get a notify below to build the DL and render.
    let mut txn = Transaction::new();
    txn.set_root_pipeline(root_pipeline_id);
    txn.generate_frame();
    api.send_transaction(document_id, txn);
    let mut time = 0.0;

    // Tick the compositor (in this sample, we don't block on UI events)
    while compositor::tick(window) {
        // If there is a new frame ready to draw
        if let Ok(..) = rx.try_recv() {
            // Update and render. This will invoke the native compositor interface implemented above
            // as required.
            renderer.update();
            renderer.render(device_size).unwrap();
            let _ = renderer.flush_pipeline_info();

            // Construct a simple display list that can be drawn and composited by DC.
            let layout_size = device_size.to_f32() / euclid::Scale::new(device_pixel_ratio);
            let mut txn = Transaction::new();
            let mut root_builder = DisplayListBuilder::new(root_pipeline_id, layout_size);

            build_display_list(
                &mut root_builder,
                root_pipeline_id,
                layout_size,
                time,
            );

            txn.set_display_list(
                current_epoch,
                None,
                layout_size,
                root_builder.finalize(),
                true,
            );
            txn.generate_frame();
            api.send_transaction(document_id, txn);
            current_epoch.0 += 1;
            time += 0.001;
            if time > 1.0 {
                time = 0.0;
            }

            // This does nothing when native compositor is enabled
            compositor::swap_buffers(window);
        }
    }

    renderer.deinit();
    compositor::destroy_window(window);
}
