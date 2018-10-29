/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{TileOffset, TileRange, LayoutRect, LayoutSize, LayoutPoint};
use api::{DeviceUintSize, DeviceUintRect};
use euclid::{vec2, point2};
use prim_store::EdgeAaSegmentMask;

/// If repetitions are far enough apart that only one is within
/// the primitive rect, then we can simplify the parameters and
/// treat the primitive as not repeated.
/// This can let us avoid unnecessary work later to handle some
/// of the parameters.
pub fn simplify_repeated_primitive(
    stretch_size: &LayoutSize,
    tile_spacing: &mut LayoutSize,
    prim_rect: &mut LayoutRect,
) {
    let stride = *stretch_size + *tile_spacing;

    if stride.width >= prim_rect.size.width {
        tile_spacing.width = 0.0;
        prim_rect.size.width = f32::min(prim_rect.size.width, stretch_size.width);
    }
    if stride.height >= prim_rect.size.height {
        tile_spacing.height = 0.0;
        prim_rect.size.height = f32::min(prim_rect.size.height, stretch_size.height);
    }
}

pub struct Repetition {
    pub origin: LayoutPoint,
    pub edge_flags: EdgeAaSegmentMask,
}

pub struct RepetitionIterator {
    current_x: i32,
    x_count: i32,
    current_y: i32,
    y_count: i32,
    row_flags: EdgeAaSegmentMask,
    current_origin: LayoutPoint,
    initial_origin: LayoutPoint,
    stride: LayoutSize,
}

impl Iterator for RepetitionIterator {
    type Item = Repetition;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current_x == self.x_count {
            self.current_y += 1;
            if self.current_y >= self.y_count {
                return None;
            }
            self.current_x = 0;

            self.row_flags = EdgeAaSegmentMask::empty();
            if self.current_y == self.y_count - 1 {
                self.row_flags |= EdgeAaSegmentMask::BOTTOM;
            }

            self.current_origin.x = self.initial_origin.x;
            self.current_origin.y += self.stride.height;
        }

        let mut edge_flags = self.row_flags;
        if self.current_x == 0 {
            edge_flags |= EdgeAaSegmentMask::LEFT;
        }

        if self.current_x == self.x_count - 1 {
            edge_flags |= EdgeAaSegmentMask::RIGHT;
        }

        let repetition = Repetition {
            origin: self.current_origin,
            edge_flags: edge_flags,
        };

        self.current_origin.x += self.stride.width;
        self.current_x += 1;

        Some(repetition)
    }
}

pub fn repetitions(
    prim_rect: &LayoutRect,
    visible_rect: &LayoutRect,
    stride: LayoutSize,
) -> RepetitionIterator {
    assert!(stride.width > 0.0);
    assert!(stride.height > 0.0);

    let visible_rect = match prim_rect.intersection(&visible_rect) {
        Some(rect) => rect,
        None => {
            return RepetitionIterator {
                current_origin: LayoutPoint::zero(),
                initial_origin: LayoutPoint::zero(),
                current_x: 0,
                current_y: 0,
                x_count: 0,
                y_count: 0,
                stride,
                row_flags: EdgeAaSegmentMask::empty(),
            }
        }
    };

    let nx = if visible_rect.origin.x > prim_rect.origin.x {
        f32::floor((visible_rect.origin.x - prim_rect.origin.x) / stride.width)
    } else {
        0.0
    };

    let ny = if visible_rect.origin.y > prim_rect.origin.y {
        f32::floor((visible_rect.origin.y - prim_rect.origin.y) / stride.height)
    } else {
        0.0
    };

    let x0 = prim_rect.origin.x + nx * stride.width;
    let y0 = prim_rect.origin.y + ny * stride.height;

    let x_most = visible_rect.max_x();
    let y_most = visible_rect.max_y();

    let x_count = f32::ceil((x_most - x0) / stride.width) as i32;
    let y_count = f32::ceil((y_most - y0) / stride.height) as i32;

    let mut row_flags = EdgeAaSegmentMask::TOP;
    if y_count == 1 {
        row_flags |= EdgeAaSegmentMask::BOTTOM;
    }

    RepetitionIterator {
        current_origin: LayoutPoint::new(x0, y0),
        initial_origin: LayoutPoint::new(x0, y0),
        current_x: 0,
        current_y: 0,
        x_count,
        y_count,
        row_flags,
        stride,
    }
}

#[derive(Debug)]
pub struct Tile {
    pub rect: LayoutRect,
    pub offset: TileOffset,
    pub edge_flags: EdgeAaSegmentMask,
}

pub struct TileIterator {
    current_x: u32,
    x_count: u32,
    current_y: u32,
    y_count: u32,
    origin: TileOffset,
    tile_size: LayoutSize,
    leftover_offset: TileOffset,
    leftover_size: LayoutSize,
    local_origin: LayoutPoint,
    row_flags: EdgeAaSegmentMask,
}

impl Iterator for TileIterator {
    type Item = Tile;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current_x == self.x_count {
            self.current_y += 1;
            if self.current_y >= self.y_count {
                return None;
            }
            self.current_x = 0;
            self.row_flags = EdgeAaSegmentMask::empty();
            if self.current_y == self.y_count - 1 {
                self.row_flags |= EdgeAaSegmentMask::BOTTOM;
            }
        }

        let tile_offset = self.origin + vec2(self.current_x, self.current_y);

        let mut segment_rect = LayoutRect {
            origin: LayoutPoint::new(
                self.local_origin.x + tile_offset.x as f32 * self.tile_size.width,
                self.local_origin.y + tile_offset.y as f32 * self.tile_size.height,
            ),
            size: self.tile_size,
        };

        if tile_offset.x == self.leftover_offset.x {
            segment_rect.size.width = self.leftover_size.width;
        }

        if tile_offset.y == self.leftover_offset.y {
            segment_rect.size.height = self.leftover_size.height;
        }

        let mut edge_flags = self.row_flags;
        if self.current_x == 0 {
            edge_flags |= EdgeAaSegmentMask::LEFT;
        }

        if self.current_x == self.x_count - 1 {
            edge_flags |= EdgeAaSegmentMask::RIGHT;
        }

        let tile = Tile {
            rect: segment_rect,
            offset: tile_offset,
            edge_flags,
        };

        self.current_x += 1;
        Some(tile)
    }
}

pub fn tiles(
    prim_rect: &LayoutRect,
    visible_rect: &LayoutRect,
    device_image_size: &DeviceUintSize,
    device_tile_size: u32,
) -> TileIterator {
    // The image resource is tiled. We have to generate an image primitive
    // for each tile.
    // We need to do this because the image is broken up into smaller tiles in the texture
    // cache and the image shader is not able to work with this type of sparse representation.

    // The tiling logic works as follows:
    //
    //  ###################-+  -+
    //  #    |    |    |//# |   | image size
    //  #    |    |    |//# |   |
    //  #----+----+----+--#-+   |  -+
    //  #    |    |    |//# |   |   | regular tile size
    //  #    |    |    |//# |   |   |
    //  #----+----+----+--#-+   |  -+-+
    //  #////|////|////|//# |   |     | "leftover" height
    //  ################### |  -+  ---+
    //  #----+----+----+----+
    //
    // In the ascii diagram above, a large image is split into tiles of almost regular size.
    // The tiles on the right and bottom edges (hatched in the diagram) are smaller than
    // the regular tiles and are handled separately in the code see leftover_width/height.
    // each generated segment corresponds to a tile in the texture cache, with the
    // assumption that the smaller tiles with leftover sizes are sized to fit their own
    // irregular size in the texture cache.

    // Because we can have very large virtual images we iterate over the visible portion of
    // the image in layer space intead of iterating over device tiles.

    let visible_rect = match prim_rect.intersection(&visible_rect) {
        Some(rect) => rect,
        None => {
            return TileIterator {
                current_x: 0,
                current_y: 0,
                x_count: 0,
                y_count: 0,
                row_flags: EdgeAaSegmentMask::empty(),
                origin: TileOffset::zero(),
                tile_size: LayoutSize::zero(),
                leftover_offset: TileOffset::zero(),
                leftover_size: LayoutSize::zero(),
                local_origin: LayoutPoint::zero(),
            }
        }
    };

    let device_tile_size_f32 = device_tile_size as f32;

    // Ratio between (image space) tile size and image size .
    let tile_dw = device_tile_size_f32 / (device_image_size.width as f32);
    let tile_dh = device_tile_size_f32 / (device_image_size.height as f32);

    // size of regular tiles in layout space.
    let layer_tile_size = LayoutSize::new(
        tile_dw * prim_rect.size.width,
        tile_dh * prim_rect.size.height,
    );

    // The size in pixels of the tiles on the right and bottom edges, smaller
    // than the regular tile size if the image is not a multiple of the tile size.
    // Zero means the image size is a multiple of the tile size.
    let leftover_device_size = DeviceUintSize::new(
        device_image_size.width % device_tile_size,
        device_image_size.height % device_tile_size
    );

    // The size in layer space of the tiles on the right and bottom edges.
    let leftover_layer_size = LayoutSize::new(
        layer_tile_size.width * leftover_device_size.width as f32 / device_tile_size_f32,
        layer_tile_size.height * leftover_device_size.height as f32 / device_tile_size_f32,
    );

    // Offset of the row and column of tiles with leftover size.
    let leftover_offset = TileOffset::new(
        (device_image_size.width / device_tile_size) as u32,
        (device_image_size.height / device_tile_size) as u32,
    );

    // Number of culled out tiles to skip before the first visible tile.
    let t0 = TileOffset::new(
        if visible_rect.origin.x > prim_rect.origin.x {
            f32::floor((visible_rect.origin.x - prim_rect.origin.x) / layer_tile_size.width) as u32
        } else {
            0
        },
        if visible_rect.origin.y > prim_rect.origin.y {
            f32::floor((visible_rect.origin.y - prim_rect.origin.y) / layer_tile_size.height) as u32
        } else {
            0
        },
    );

    // Since we're working in layer space, we can end up computing leftover tiles with an empty
    // size due to floating point precision issues. Detect this case so that we don't return
    // tiles with an empty size.
    let x_count = {
        let result = f32::ceil((visible_rect.max_x() - prim_rect.origin.x) / layer_tile_size.width) as u32 - t0.x;
        if result == leftover_offset.x + 1 && leftover_layer_size.width == 0.0f32 {
            leftover_offset.x
        } else {
            result
        }
    };
    let y_count = {
        let result = f32::ceil((visible_rect.max_y() - prim_rect.origin.y) / layer_tile_size.height) as u32 - t0.y;
        if result == leftover_offset.y + 1 && leftover_layer_size.height == 0.0f32 {
            leftover_offset.y
        } else {
            result
        }
    };

    let mut row_flags = EdgeAaSegmentMask::TOP;
    if y_count == 1 {
        row_flags |= EdgeAaSegmentMask::BOTTOM;
    }
    TileIterator {
        current_x: 0,
        current_y: 0,
        x_count,
        y_count,
        row_flags,
        origin: t0,
        tile_size: layer_tile_size,
        leftover_offset,
        leftover_size: leftover_layer_size,
        local_origin: prim_rect.origin,
    }
}

pub fn compute_tile_range(
    visible_area: &DeviceUintRect,
    tile_size: u16,
) -> TileRange {
    // Tile dimensions in normalized coordinates.
    let tw = 1. / (tile_size as f32);
    let th = 1. / (tile_size as f32);

    let t0 = point2(
        f32::floor(visible_area.origin.x as f32 * tw),
        f32::floor(visible_area.origin.y as f32 * th),
    ).try_cast::<u32>().unwrap_or_else(|| panic!("compute_tile_range bad values {:?} {:?}", visible_area, tile_size));

    let t1 = point2(
        f32::ceil(visible_area.max_x() as f32 * tw),
        f32::ceil(visible_area.max_y() as f32 * th),
    ).try_cast::<u32>().unwrap_or_else(|| panic!("compute_tile_range bad values {:?} {:?}", visible_area, tile_size));

    TileRange {
        origin: t0,
        size: (t1 - t0).to_size(),
    }
}

pub fn for_each_tile_in_range(
    range: &TileRange,
    mut callback: impl FnMut(TileOffset),
) {
    for y in 0..range.size.height {
        for x in 0..range.size.width {
            callback(range.origin + vec2(x, y));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashSet;
    use api::{LayoutRect, DeviceUintSize};
    use euclid::{rect, size2};

    // this checks some additional invariants
    fn checked_for_each_tile(
        prim_rect: &LayoutRect,
        visible_rect: &LayoutRect,
        device_image_size: &DeviceUintSize,
        device_tile_size: u32,
        callback: &mut FnMut(&LayoutRect, TileOffset, EdgeAaSegmentMask),
    ) {
        let mut coverage = LayoutRect::zero();
        let mut seen_tiles = HashSet::new();
        for tile in tiles(
            prim_rect,
            visible_rect,
            device_image_size,
            device_tile_size,
        ) {
            // make sure we don't get sent duplicate tiles
            assert!(!seen_tiles.contains(&tile.offset));
            seen_tiles.insert(tile.offset);
            coverage = coverage.union(&tile.rect);
            assert!(prim_rect.contains_rect(&tile.rect));
            callback(&tile.rect, tile.offset, tile.edge_flags);
        }
        assert!(prim_rect.contains_rect(&coverage));
        assert!(coverage.contains_rect(&visible_rect.intersection(&prim_rect).unwrap_or(LayoutRect::zero())));
    }

    #[test]
    fn basic() {
        let mut count = 0;
        checked_for_each_tile(&rect(0., 0., 1000., 1000.),
            &rect(75., 75., 400., 400.),
            &size2(400, 400),
            36,
            &mut |_tile_rect, _tile_offset, _tile_flags| {
                count += 1;
            },
        );
        assert_eq!(count, 36);
    }

    #[test]
    fn empty() {
        let mut count = 0;
        checked_for_each_tile(&rect(0., 0., 74., 74.),
              &rect(75., 75., 400., 400.),
              &size2(400, 400),
              36,
              &mut |_tile_rect, _tile_offset, _tile_flags| {
                count += 1;
              },
        );
        assert_eq!(count, 0);
    }
}
