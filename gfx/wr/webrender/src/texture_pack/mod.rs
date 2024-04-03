/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod guillotine;
use crate::texture_cache::TextureCacheHandle;
use crate::internal_types::FastHashMap;
pub use guillotine::*;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::units::*;
use crate::internal_types::CacheTextureId;
use euclid::{point2, size2, default::Box2D};
use smallvec::SmallVec;

pub use etagere::AllocatorOptions as ShelfAllocatorOptions;
pub use etagere::BucketedAtlasAllocator as BucketedShelfAllocator;
pub use etagere::AtlasAllocator as ShelfAllocator;

/// ID of an allocation within a given allocator.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct AllocId(pub u32);

pub trait AtlasAllocator {
    /// Specific parameters of the allocator.
    type Parameters;
    /// Constructor
    fn new(size: i32, parameters: &Self::Parameters) -> Self;
    /// Allocate a rectangle.
    fn allocate(&mut self, size: DeviceIntSize) -> Option<(AllocId, DeviceIntRect)>;
    /// Deallocate a rectangle and return its size.
    fn deallocate(&mut self, id: AllocId);
    /// Return true if there is no live allocations.
    fn is_empty(&self) -> bool;
    /// Allocated area in pixels.
    fn allocated_space(&self) -> i32;
    /// Write a debug visualization of the atlas fitting in the provided rectangle.
    ///
    /// This is inserted in a larger dump so it shouldn't contain the xml start/end tags.
    fn dump_into_svg(&self, rect: &Box2D<f32>, output: &mut dyn std::io::Write) -> std::io::Result<()>;
}

pub trait AtlasAllocatorList<TextureParameters> {
    /// Allocate a rectangle.
    ///
    /// If allocation fails, call the provided callback, add a new allocator to the list and try again.
    fn allocate(
        &mut self,
        size: DeviceIntSize,
        texture_alloc_cb: &mut dyn FnMut(DeviceIntSize, &TextureParameters) -> CacheTextureId,
    ) -> (CacheTextureId, AllocId, DeviceIntRect);

    fn set_handle(&mut self, texture_id: CacheTextureId, alloc_id: AllocId, handle: &TextureCacheHandle);

    /// Deallocate a rectangle and return its size.
    fn deallocate(&mut self, texture_id: CacheTextureId, alloc_id: AllocId);

    fn texture_parameters(&self) -> &TextureParameters;
}

/// A number of 2D textures (single layer), with their own atlas allocator.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureUnit<Allocator> {
    allocator: Allocator,
    handles: FastHashMap<AllocId, TextureCacheHandle>,
    texture_id: CacheTextureId,
    // The texture might become empty during a frame where we copy items out
    // of it, in which case we want to postpone deleting the texture to the
    // next frame.
    delay_deallocation: bool,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct AllocatorList<Allocator: AtlasAllocator, TextureParameters> {
    units: SmallVec<[TextureUnit<Allocator>; 1]>,
    size: i32,
    atlas_parameters: Allocator::Parameters,
    texture_parameters: TextureParameters,
}

impl<Allocator: AtlasAllocator, TextureParameters> AllocatorList<Allocator, TextureParameters> {
    pub fn new(
        size: i32,
        atlas_parameters: Allocator::Parameters,
        texture_parameters: TextureParameters,
    ) -> Self {
        AllocatorList {
            units: SmallVec::new(),
            size,
            atlas_parameters,
            texture_parameters,
        }
    }

    pub fn allocate(
        &mut self,
        requested_size: DeviceIntSize,
        texture_alloc_cb: &mut dyn FnMut(DeviceIntSize, &TextureParameters) -> CacheTextureId,
    ) -> (CacheTextureId, AllocId, DeviceIntRect) {
        // Try to allocate from one of the existing textures.
        for unit in &mut self.units {
            if let Some((alloc_id, rect)) = unit.allocator.allocate(requested_size) {
                return (unit.texture_id, alloc_id, rect);
            }
        }

        // Need to create a new texture to hold the allocation.
        let texture_id = texture_alloc_cb(size2(self.size, self.size), &self.texture_parameters);
        let unit_index = self.units.len();

        self.units.push(TextureUnit {
            allocator: Allocator::new(self.size, &self.atlas_parameters),
            handles: FastHashMap::default(),
            texture_id,
            delay_deallocation: false,
        });

        let (alloc_id, rect) = self.units[unit_index]
            .allocator
            .allocate(requested_size)
            .unwrap();

        (texture_id, alloc_id, rect)
    }

    pub fn deallocate(&mut self, texture_id: CacheTextureId, alloc_id: AllocId) {
        let unit = self.units
            .iter_mut()
            .find(|unit| unit.texture_id == texture_id)
            .expect("Unable to find the associated texture array unit");

        unit.handles.remove(&alloc_id);
        unit.allocator.deallocate(alloc_id);
    }

    pub fn release_empty_textures<'l>(&mut self, texture_dealloc_cb: &'l mut dyn FnMut(CacheTextureId)) {
        self.units.retain(|unit| {
            if unit.allocator.is_empty() && !unit.delay_deallocation {
                texture_dealloc_cb(unit.texture_id);

                false
            } else{
                unit.delay_deallocation = false;
                true
            }
        });
    }

    pub fn clear(&mut self, texture_dealloc_cb: &mut dyn FnMut(CacheTextureId)) {
        for unit in self.units.drain(..) {
            texture_dealloc_cb(unit.texture_id);
        }
    }

    #[allow(dead_code)]
    pub fn dump_as_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        use svg_fmt::*;

        let num_arrays = self.units.len() as f32;

        let text_spacing = 15.0;
        let unit_spacing = 30.0;
        let texture_size = self.size as f32 / 2.0;

        let svg_w = unit_spacing * 2.0 + texture_size;
        let svg_h = unit_spacing + num_arrays * (texture_size + text_spacing + unit_spacing);

        writeln!(output, "{}", BeginSvg { w: svg_w, h: svg_h })?;

        // Background.
        writeln!(output,
            "    {}",
            rectangle(0.0, 0.0, svg_w, svg_h)
                .inflate(1.0, 1.0)
                .fill(rgb(50, 50, 50))
        )?;

        let mut y = unit_spacing;
        for unit in &self.units {
            writeln!(output, "    {}", text(unit_spacing, y, format!("{:?}", unit.texture_id)).color(rgb(230, 230, 230)))?;

            let rect = Box2D {
                min: point2(unit_spacing, y),
                max: point2(unit_spacing + texture_size, y + texture_size),
            };

            unit.allocator.dump_into_svg(&rect, output)?;

            y += unit_spacing + texture_size + text_spacing;
        }

        writeln!(output, "{}", EndSvg)
    }

    pub fn allocated_space(&self) -> i32 {
        let mut accum = 0;
        for unit in &self.units {
            accum += unit.allocator.allocated_space();
        }

        accum
    }

    pub fn allocated_textures(&self) -> usize {
        self.units.len()
    }

    pub fn size(&self) -> i32 { self.size }
}

impl<Allocator: AtlasAllocator, TextureParameters> AtlasAllocatorList<TextureParameters> 
for AllocatorList<Allocator, TextureParameters> {
    fn allocate(
        &mut self,
        requested_size: DeviceIntSize,
        texture_alloc_cb: &mut dyn FnMut(DeviceIntSize, &TextureParameters) -> CacheTextureId,
    ) -> (CacheTextureId, AllocId, DeviceIntRect) {
        self.allocate(requested_size, texture_alloc_cb)
    }

    fn set_handle(&mut self, texture_id: CacheTextureId, alloc_id: AllocId, handle: &TextureCacheHandle) {
        let unit = self.units
            .iter_mut()
            .find(|unit| unit.texture_id == texture_id)
            .expect("Unable to find the associated texture array unit");
        unit.handles.insert(alloc_id, handle.clone());
    }

    fn deallocate(&mut self, texture_id: CacheTextureId, alloc_id: AllocId) {
        self.deallocate(texture_id, alloc_id);
    }

    fn texture_parameters(&self) -> &TextureParameters {
        &self.texture_parameters
    }
}

impl AtlasAllocator for BucketedShelfAllocator {
    type Parameters = ShelfAllocatorOptions;

    fn new(size: i32, options: &Self::Parameters) -> Self {
        BucketedShelfAllocator::with_options(size2(size, size), options)
    }

    fn allocate(&mut self, size: DeviceIntSize) -> Option<(AllocId, DeviceIntRect)> {
        self.allocate(size.to_untyped()).map(|alloc| {
            (AllocId(alloc.id.serialize()), alloc.rectangle.cast_unit())
        })
    }

    fn deallocate(&mut self, id: AllocId) {
        self.deallocate(etagere::AllocId::deserialize(id.0));
    }

    fn is_empty(&self) -> bool {
        self.is_empty()
    }

    fn allocated_space(&self) -> i32 {
        self.allocated_space()
    }

    fn dump_into_svg(&self, rect: &Box2D<f32>, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.dump_into_svg(Some(&rect.to_i32().cast_unit()), output)
    }
}

impl AtlasAllocator for ShelfAllocator {
    type Parameters = ShelfAllocatorOptions;

    fn new(size: i32, options: &Self::Parameters) -> Self {
        ShelfAllocator::with_options(size2(size, size), options)
    }

    fn allocate(&mut self, size: DeviceIntSize) -> Option<(AllocId, DeviceIntRect)> {
        self.allocate(size.to_untyped()).map(|alloc| {
            (AllocId(alloc.id.serialize()), alloc.rectangle.cast_unit())
        })
    }

    fn deallocate(&mut self, id: AllocId) {
        self.deallocate(etagere::AllocId::deserialize(id.0));
    }

    fn is_empty(&self) -> bool {
        self.is_empty()
    }

    fn allocated_space(&self) -> i32 {
        self.allocated_space()
    }

    fn dump_into_svg(&self, rect: &Box2D<f32>, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        self.dump_into_svg(Some(&rect.to_i32().cast_unit()), output)
    }
}

pub struct CompactionChange {
    pub handle: TextureCacheHandle,
    pub old_tex: CacheTextureId,
    pub old_rect: DeviceIntRect,
    pub new_id: AllocId,
    pub new_tex: CacheTextureId,
    pub new_rect: DeviceIntRect,
}

impl<P> AllocatorList<ShelfAllocator, P> {
    /// Attempt to move some allocations from a texture to another to reduce the number of textures.
    pub fn try_compaction(
        &mut self,
        max_pixels: i32,
        changes: &mut Vec<CompactionChange>,
    ) {
        // The goal here is to consolidate items in the first texture by moving them from the last.

        if self.units.len() < 2 {
            // Nothing to do we are already "compact".
            return;
        }

        let last_unit = self.units.len() - 1;
        let mut pixels = 0;
        while let Some(alloc) = self.units[last_unit].allocator.iter().next() {
            // For each allocation in the last texture, try to allocate it in the first one.
            let new_alloc = match self.units[0].allocator.allocate(alloc.rectangle.size()) {
                Some(new_alloc) => new_alloc,
                None => {
                    // Stop when we fail to fit an item into the first texture.
                    // We could potentially fit another smaller item in there but we take it as
                    // an indication that the texture is more or less full, and we'll eventually
                    // manage to move the items later if they still exist as other items expire,
                    // which is what matters.
                    break;
                }
            };

            // The item was successfully reallocated in the first texture, we can proceed
            // with removing it from the last.

            // We keep track of the texture cache handle for each allocation, make sure
            // the new allocation has the proper handle.
            let alloc_id = AllocId(alloc.id.serialize());
            let new_alloc_id = AllocId(new_alloc.id.serialize());
            let handle = self.units[last_unit].handles.get(&alloc_id).unwrap().clone();
            self.units[0].handles.insert(new_alloc_id, handle.clone());

            // Remove the allocation for the last texture.
            self.units[last_unit].handles.remove(&alloc_id);
            self.units[last_unit].allocator.deallocate(alloc.id);

            // Prevent the texture from being deleted on the same frame.
            self.units[last_unit].delay_deallocation = true;

            // Record the change so that the texture cache can do additional bookkeeping.
            changes.push(CompactionChange {
                handle,
                old_tex: self.units[last_unit].texture_id,
                old_rect: alloc.rectangle.cast_unit(),
                new_id: AllocId(new_alloc.id.serialize()),
                new_tex: self.units[0].texture_id,
                new_rect: new_alloc.rectangle.cast_unit(),
            });

            // We are not in a hurry to move all allocations we can in one go, as long as we
            // eventually have a chance to move them all within a reasonable amount of time.
            // It's best to spread the load over multiple frames to avoid sudden spikes, so we
            // stop after we have passed a certain threshold.
            pixels += alloc.rectangle.area();
            if pixels > max_pixels {
                break;
            }
        }
    }

}

#[test]
fn bug_1680769() {
    let mut allocators: AllocatorList<ShelfAllocator, ()> = AllocatorList::new(
        1024,
        ShelfAllocatorOptions::default(),
        (),
    );

    let mut allocations = Vec::new();
    let mut next_id = CacheTextureId(0);
    let alloc_cb = &mut |_: DeviceIntSize, _: &()| {
        let texture_id = next_id;
        next_id.0 += 1;

        texture_id
    };

    // Make some allocations, forcing the the creation of multiple textures.
    for _ in 0..50 {
        let alloc = allocators.allocate(size2(256, 256), alloc_cb);
        allocators.set_handle(alloc.0, alloc.1, &TextureCacheHandle::Empty);
        allocations.push(alloc);
    }

    // Deallocate everything.
    // It should empty all atlases and we still have textures allocated because
    // we haven't called release_empty_textures yet.
    for alloc in allocations.drain(..) {
        allocators.deallocate(alloc.0, alloc.1);
    }

    // Allocate something else.
    // Bug 1680769 was causing this allocation to be duplicated and leaked in
    // all textures.
    allocations.push(allocators.allocate(size2(8, 8), alloc_cb));

    // Deallocate all known allocations.
    for alloc in allocations.drain(..) {
        allocators.deallocate(alloc.0, alloc.1);
    }

    // If we have leaked items, this won't manage to remove all textures.
    allocators.release_empty_textures(&mut |_| {});

    assert_eq!(allocators.allocated_textures(), 0);
}
