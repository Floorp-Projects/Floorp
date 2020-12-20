// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use api::units::*;
use api::ImageFormat;
use crate::composite::{NativeTileId, NativeSurfaceId};
use crate::frame_graph::FrameGraphBuilder;
use crate::gpu_cache::GpuCache;
use crate::internal_types::CacheTextureId;
use crate::picture::{ResolvedSurfaceTexture};
use crate::resource_cache::ResourceCache;
use crate::render_task::{RenderTask, StaticRenderTaskSurface, RenderTaskLocation};
use crate::render_target::RenderTargetKind;


/// Construct a picture cache render task location for testing
fn pc_target(
    surface_id: u64,
    tile_x: i32,
    tile_y: i32,
) -> RenderTaskLocation {
    let width = 512;
    let height = 512;

    RenderTaskLocation::Static {
        surface: StaticRenderTaskSurface::PictureCache {
            surface: ResolvedSurfaceTexture::Native {
                id: NativeTileId {
                    surface_id: NativeSurfaceId(surface_id),
                    x: tile_x,
                    y: tile_y,
                },
                size: DeviceIntSize::new(width, height),
            },
        },
        rect: DeviceIntSize::new(width, height).into(),
    }
}

/// Construct a testing render task with given location
fn task_location(location: RenderTaskLocation) -> RenderTask {
    RenderTask::new_test(
        location,
        RenderTargetKind::Color,
    )
}

/// Construct a dynamic render task location for testing
fn task_dynamic(size: i32) -> RenderTask {
    RenderTask::new_test(
        RenderTaskLocation::Unallocated { size: DeviceIntSize::new(size, size) },
        RenderTargetKind::Color,
    )
}

#[test]
fn fg_test_1() {
    // Test that a root target can be used as an input for readbacks
    // This functionality isn't currently used, but will be in future.

    let (mut gb, mut rc, mut gc) = setup_test();

    let root_target = pc_target(0, 0, 0);

    let root = gb.add().init(task_location(root_target.clone()));

    let readback = gb.add().init(task_dynamic(100));
    gb.add_dependency(readback, root);

    let mix_blend_content = gb.add().init(task_dynamic(50));

    let content = gb.add().init(task_location(root_target));
    gb.add_dependency(content, readback);
    gb.add_dependency(content, mix_blend_content);

    let g = gb.end_frame(&mut rc, &mut gc);
    g.print();

    assert_eq!(g.passes.len(), 3);
    assert_eq!(g.surface_counts(), (1, 1));

    rc.validate_surfaces(&[
        (2048, 2048, ImageFormat::RGBA8),
    ]);
}

#[test]
fn fg_test_2() {
    // Test that texture cache tasks can be added and scheduled correctly as inputs
    // to picture cache tasks. Ensure that no dynamic surfaces are allocated from the
    // target pool in this case.

    let (mut gb, mut rc, mut gc) = setup_test();

    let pc_root = gb.add().init(task_location(pc_target(0, 0, 0)));

    let tc_0 = StaticRenderTaskSurface::TextureCache {
        texture: CacheTextureId(0),
        layer: 0,
        target_kind: RenderTargetKind::Color,
    };

    let tc_1 = StaticRenderTaskSurface::TextureCache {
        texture: CacheTextureId(1),
        layer: 0,
        target_kind: RenderTargetKind::Color,
    };

    gb.add_target_input(
        pc_root,
        tc_0.clone(),
    );

    gb.add_target_input(
        pc_root,
        tc_1.clone(),
    );

    gb.add().init(
        RenderTask::new_test(
            RenderTaskLocation::Static { surface: tc_0.clone(), rect: DeviceIntSize::new(128, 128).into() },
            RenderTargetKind::Color,
        )
    );

    gb.add().init(
        RenderTask::new_test(
            RenderTaskLocation::Static { surface: tc_1.clone(), rect: DeviceIntSize::new(128, 128).into() },
            RenderTargetKind::Color,
        )
    );

    let g = gb.end_frame(&mut rc, &mut gc);
    g.print();

    assert_eq!(g.passes.len(), 2);
    assert_eq!(g.surface_counts(), (0, 0));

    rc.validate_surfaces(&[]);
}

#[test]
fn fg_test_3() {
    // Test that small targets are allocated in a shared surface, and that large
    // tasks are allocated in a rounded up texture size.

    let (mut gb, mut rc, mut gc) = setup_test();

    let pc_root = gb.add().init(task_location(pc_target(0, 0, 0)));

    let child_pic_0 = gb.add().init(task_dynamic(128));
    let child_pic_1 = gb.add().init(task_dynamic(3000));

    gb.add_dependency(pc_root, child_pic_0);
    gb.add_dependency(pc_root, child_pic_1);

    let g = gb.end_frame(&mut rc, &mut gc);
    g.print();

    assert_eq!(g.passes.len(), 2);
    assert_eq!(g.surface_counts(), (2, 2));

    rc.validate_surfaces(&[
        (2048, 2048, ImageFormat::RGBA8),
        (3072, 3072, ImageFormat::RGBA8),
    ]);
}

#[test]
fn fg_test_4() {
    // Test that for a simple dependency chain of tasks, that render
    // target surfaces are aliased and reused between passes where possible.

    let (mut gb, mut rc, mut gc) = setup_test();

    let pc_root = gb.add().init(task_location(pc_target(0, 0, 0)));

    let child_pic_0 = gb.add().init(task_dynamic(128));
    let child_pic_1 = gb.add().init(task_dynamic(128));
    let child_pic_2 = gb.add().init(task_dynamic(128));

    gb.add_dependency(pc_root, child_pic_0);
    gb.add_dependency(child_pic_0, child_pic_1);
    gb.add_dependency(child_pic_1, child_pic_2);

    let g = gb.end_frame(&mut rc, &mut gc);
    g.print();

    assert_eq!(g.passes.len(), 4);
    assert_eq!(g.surface_counts(), (3, 2));

    rc.validate_surfaces(&[
        (2048, 2048, ImageFormat::RGBA8),
        (2048, 2048, ImageFormat::RGBA8),
    ]);
}

#[test]
fn fg_test_5() {
    // Test that a task that is used as an input by direct parent and also
    // distance ancestor are scheduled correctly, and allocates the correct
    // number of passes, taking advantage of surface reuse / aliasing where feasible.

    let (mut gb, mut rc, mut gc) = setup_test();

    let pc_root = gb.add().init(task_location(pc_target(0, 0, 0)));

    let child_pic_0 = gb.add().init(task_dynamic(128));
    let child_pic_1 = gb.add().init(task_dynamic(64));
    let child_pic_2 = gb.add().init(task_dynamic(32));
    let child_pic_3 = gb.add().init(task_dynamic(16));

    gb.add_dependency(pc_root, child_pic_0);
    gb.add_dependency(child_pic_0, child_pic_1);
    gb.add_dependency(child_pic_1, child_pic_2);
    gb.add_dependency(child_pic_2, child_pic_3);
    gb.add_dependency(pc_root, child_pic_3);

    let g = gb.end_frame(&mut rc, &mut gc);

    g.print();

    assert_eq!(g.passes.len(), 5);
    assert_eq!(g.surface_counts(), (4, 3));

    rc.validate_surfaces(&[
        (256, 256, ImageFormat::RGBA8),
        (2048, 2048, ImageFormat::RGBA8),
        (2048, 2048, ImageFormat::RGBA8),
    ]);
}

#[test]
fn fg_test_6() {
    // Test that a task that is used as an input dependency by two parent
    // tasks is correctly allocated and freed.

    let (mut gb, mut rc, mut gc) = setup_test();

    let pc_root_1 = gb.add().init(task_location(pc_target(0, 0, 0)));
    let pc_root_2 = gb.add().init(task_location(pc_target(0, 1, 0)));

    let child_pic = gb.add().init(task_dynamic(128));

    gb.add_dependency(pc_root_1, child_pic);
    gb.add_dependency(pc_root_2, child_pic);

    let g = gb.end_frame(&mut rc, &mut gc);
    g.print();

    assert_eq!(g.passes.len(), 2);
    assert_eq!(g.surface_counts(), (1, 1));

    rc.validate_surfaces(&[
        (2048, 2048, ImageFormat::RGBA8),
    ]);
}

#[test]
fn fg_test_7() {
    // Test that a standalone surface is not incorrectly used to
    // allocate subsequent shared task rects.

    let (mut gb, mut rc, mut gc) = setup_test();

    let pc_root = gb.add().init(task_location(pc_target(0, 0, 0)));

    let child0 = gb.add().init(task_dynamic(16));
    let child1 = gb.add().init(task_dynamic(16));

    let child2 = gb.add().init(task_dynamic(16));
    let child3 = gb.add().init(task_dynamic(16));

    gb.add_dependency(pc_root, child0);
    gb.add_dependency(child0, child1);
    gb.add_dependency(pc_root, child1);

    gb.add_dependency(pc_root, child2);
    gb.add_dependency(child2, child3);

    let g = gb.end_frame(&mut rc, &mut gc);
    g.print();

    assert_eq!(g.passes.len(), 3);
    assert_eq!(g.surface_counts(), (3, 3));
    rc.validate_surfaces(&[
        (256, 256, ImageFormat::RGBA8),
        (2048, 2048, ImageFormat::RGBA8),
        (2048, 2048, ImageFormat::RGBA8),
    ]);
}

fn setup_test() -> (FrameGraphBuilder, ResourceCache, GpuCache) {
    let gb = FrameGraphBuilder::new();
    let rc = ResourceCache::new_for_testing();
    let gc = GpuCache::new();

    (gb, rc, gc)
}
