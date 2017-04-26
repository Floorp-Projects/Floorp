# Clipping in WebRender

The WebRender display list allows defining clips in two different ways. The
first is specified directly on each display item and cannot be reused between
items. The second is specified using the `SpecificDisplayItem::Clip` display item
and can be reused between items as well as used to define scrollable regions.

## Clips

Clips are defined using the ClipRegion in both cases.

```rust
pub struct ClipRegion {
    pub main: LayoutRect,
    pub complex: ItemRange,
    pub image_mask: Option<ImageMask>,
}
```

`main` defines a rectangular clip, while the other members make that rectangle
smaller. `complex`, if it is not empty, defines the boundaries of a rounded
rectangle. While `image_mask` defines the positioning, repetition, and data of
a masking image.

## Item Clips

Item clips are simply a `ClipRegion` structure defined directly on the
`DisplayItem`. The important thing to note about these clips is that all the
coordinate in `ClipRegion` **are in the same coordinate space as the item
itself**. This different than for clips defined by `SpecificDisplayItem::Clip`.

## Clip Display Items

Clip display items allow items to share clips in order to increase performance
(shared clips are only rasterized once) and to allow for scrolling regions.
Display items can be assigned a clip display item using the `clip_id`
field. An item can be assigned any clip that is defined by its parent stacking
context or any of the ancestors. The behavior of assigning an id outside of
this hierarchy is undefined, because that situation does not occur in CSS

The clip display item has a `ClipRegion` as well as several other fields:

```rust
pub struct ClipDisplayItem {
    pub id: ClipId,
    pub parent_id: ClipId,
}
```

A `ClipDisplayItem` also gets a clip and bounds from the `BaseDisplayItem`. The
clip is shared among all items that use this `ClipDisplayItem`. Critically,
**coordinates in this ClipRegion are defined relative to the bounds of the
ClipDisplayItem itself**. Additionally, WebRender only supports clip rectangles
that start at the origin of the `BaseDisplayItem` bounds.

The `BaseDisplayItem` bounds are known as the *content rectangle* of the clip. If
the content rectangle is larger than *main* clipping rectangle, the clip will
be a scrolling clip and participate in scrolling event capture and
transformation.

`ClipDisplayItems` are positioned, like all other items, relatively to their
containing stacking context, yet they also live in a parallel tree defined by
their `parent_id`. Child clips also include any clipping and scrolling that
their ancestors do. In this way, a clip is positioned by a stacking context,
but that position may be adjusted by any scroll offset of its parent clips.

## Clip ids

All clips defined by a `ClipDisplayItem` have an id. It is useful to associate
an external id with WebRender id in order to allow for tracking and updating
scroll positions using the WebRender API. In order to make this as cheap as
possible and to avoid having to create a `HashMap` to map between the two types
of ids, the WebRender API provides an optional id argument in
`DisplayListBuilder::define_clip`. The only types of ids that are supported
here are those created with `ClipId::new(...)`. If this argument is not
provided `define_clip` will return a uniquely generated id. Thus, the following
should always be true:

```rust
let id = ClipId::new(my_internal_id, pipeline_id);
let generated_id = define_clip(content_rect, clip, id);
assert!(id == generated_id);
```

Note that calling `define_clip` multiple times with the same `clip_id` value
results in undefined behaviour, and should be avoided. There is a debug mode
assertion to catch this.

## Pending changes
1. Normalize the way that clipping coordinates are defined. Having them
   specified in two different ways makes for a confusing API. This should be
   fixed.  ([github issue](https://github.com/servo/webrender/issues/1090))

1. It should be possible to specify more than one predefined clip for an item.
   This is necessary for items that live in a scrolling frame, but are also
   clipped by a clip that lives outside that frame.
   ([github issue](https://github.com/servo/webrender/issues/840))
