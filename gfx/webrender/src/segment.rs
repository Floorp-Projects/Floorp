/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipMode, LayerPoint, LayerPointAu, LayerRect, LayerSize};
use app_units::Au;
use prim_store::EdgeAaSegmentMask;
use std::cmp;
use util::extract_inner_rect_safe;

// The segment builder outputs a list of these segments.
#[derive(Debug, PartialEq)]
pub struct Segment {
    pub rect: LayerRect,
    pub has_mask: bool,
    pub edge_flags: EdgeAaSegmentMask,
}

// The segment builder creates a list of x/y axis events
// that are used to build a segment list. Right now, we
// don't bother providing a list of *which* clip regions
// are active for a given segment. Instead, if there is
// any clip mask present in a segment, we will just end
// up drawing each of the masks to that segment clip.
// This is a fairly rare case, but we can detect this
// in the future and only apply clip masks that are
// relevant to each segment region.
// TODO(gw): Provide clip region info with each segment.
#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd)]
enum EventKind {
    Begin,
    End,
}

// Events must be ordered such that when the coordinates
// of two events are the same, the end events are processed
// before the begin events. This ensures that we're able
// to detect which regions are active for a given segment.
impl Ord for EventKind {
    fn cmp(&self, other: &EventKind) -> cmp::Ordering {
        match (*self, *other) {
            (EventKind::Begin, EventKind::Begin) |
            (EventKind::End, EventKind::End) => {
                cmp::Ordering::Equal
            }
            (EventKind::Begin, EventKind::End) => {
                cmp::Ordering::Greater
            }
            (EventKind::End, EventKind::Begin) => {
                cmp::Ordering::Less
            }
        }
    }
}

// A x/y event where we will create a vertex in the
// segment builder.
#[derive(Debug, Eq, PartialEq, PartialOrd)]
struct Event {
    value: Au,
    item_index: ItemIndex,
    kind: EventKind,
}

impl Ord for Event {
    fn cmp(&self, other: &Event) -> cmp::Ordering {
        self.value
            .cmp(&other.value)
            .then(self.kind.cmp(&other.kind))
    }
}

impl Event {
    fn begin(value: f32, index: usize) -> Event {
        Event {
            value: Au::from_f32_px(value),
            item_index: ItemIndex(index),
            kind: EventKind::Begin,
        }
    }

    fn end(value: f32, index: usize) -> Event {
        Event {
            value: Au::from_f32_px(value),
            item_index: ItemIndex(index),
            kind: EventKind::End,
        }
    }

    fn is_active(&self) -> bool {
        match self.kind {
            EventKind::Begin => true,
            EventKind::End => false,
        }
    }
}

// An item that provides some kind of clip region (either
// a clip in/out rect, or a mask region).
#[derive(Debug)]
struct Item {
    rect: LayerRect,
    mode: ClipMode,
    has_mask: bool,
    active_x: bool,
    active_y: bool,
}

impl Item {
    fn new(
        rect: LayerRect,
        mode: ClipMode,
        has_mask: bool,
    ) -> Item {
        Item {
            rect,
            mode,
            has_mask,
            active_x: false,
            active_y: false,
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd)]
struct ItemIndex(usize);

// The main public interface to the segment module.
pub struct SegmentBuilder {
    items: Vec<Item>,
    bounding_rect: Option<LayerRect>,
}

impl SegmentBuilder {
    // Create a new segment builder, supplying the primitive
    // local rect and associated local clip rect.
    pub fn new(
        local_rect: LayerRect,
        local_clip_rect: LayerRect,
    ) -> SegmentBuilder {
        let mut builder = SegmentBuilder {
            items: Vec::new(),
            bounding_rect: Some(local_rect),
        };

        builder.push_rect(local_rect, None, ClipMode::Clip);
        builder.push_rect(local_clip_rect, None, ClipMode::Clip);

        builder
    }

    // Push some kind of clipping region into the segment builder.
    // If radius is None, it's a simple rect.
    pub fn push_rect(
        &mut self,
        rect: LayerRect,
        radius: Option<BorderRadius>,
        mode: ClipMode,
    ) {
        // Keep track of a minimal bounding rect for the set of
        // segments that will be generated.
        if mode == ClipMode::Clip {
            self.bounding_rect = self.bounding_rect.and_then(|bounding_rect| {
                bounding_rect.intersection(&rect)
            });
        }

        match radius {
            Some(radius) => {
                // For a rounded rect, try to create a nine-patch where there
                // is a clip item for each corner, inner and edge region.
                match extract_inner_rect_safe(&rect, &radius) {
                    Some(inner) => {
                        let p0 = rect.origin;
                        let p1 = inner.origin;
                        let p2 = inner.bottom_right();
                        let p3 = rect.bottom_right();

                        let corner_segments = &[
                            LayerRect::new(
                                LayerPoint::new(p0.x, p0.y),
                                LayerSize::new(p1.x - p0.x, p1.y - p0.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p2.x, p0.y),
                                LayerSize::new(p3.x - p2.x, p1.y - p0.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p2.x, p2.y),
                                LayerSize::new(p3.x - p2.x, p3.y - p2.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p0.x, p2.y),
                                LayerSize::new(p1.x - p0.x, p3.y - p2.y),
                            ),
                        ];

                        for segment in corner_segments {
                            self.items.push(Item::new(
                                *segment,
                                mode,
                                true
                            ));
                        }

                        let other_segments = &[
                            LayerRect::new(
                                LayerPoint::new(p1.x, p0.y),
                                LayerSize::new(p2.x - p1.x, p1.y - p0.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p2.x, p1.y),
                                LayerSize::new(p3.x - p2.x, p2.y - p1.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p1.x, p2.y),
                                LayerSize::new(p2.x - p1.x, p3.y - p2.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p0.x, p1.y),
                                LayerSize::new(p1.x - p0.x, p2.y - p1.y),
                            ),
                            LayerRect::new(
                                LayerPoint::new(p1.x, p1.y),
                                LayerSize::new(p2.x - p1.x, p2.y - p1.y),
                            ),
                        ];

                        for segment in other_segments {
                            self.items.push(Item::new(
                                *segment,
                                mode,
                                false,
                            ));
                        }
                    }
                    None => {
                        // If we get here, we could not extract an inner rectangle
                        // for this clip region. This can occur in cases such as
                        // a rounded rect where the top-left and bottom-left radii
                        // result in overlapping rects. In that case, just create
                        // a single clip region for the entire rounded rect.
                        self.items.push(Item::new(
                            rect,
                            mode,
                            true,
                        ))
                    }
                }
            }
            None => {
                // For a simple rect, just create one clipping item.
                self.items.push(Item::new(
                    rect,
                    mode,
                    false,
                ))
            }
        }
    }

    // Consume this segment builder and produce a list of segments.
    pub fn build<F>(self, mut f: F) where F: FnMut(Segment) {
        let bounding_rect = match self.bounding_rect {
            Some(bounding_rect) => bounding_rect,
            None => return,
        };

        // First, filter out any items that don't intersect
        // with the visible bounding rect.
        let mut items: Vec<Item> = self.items
            .into_iter()
            .filter(|item| item.rect.intersects(&bounding_rect))
            .collect();

        // Create events for each item
        let mut x_events = Vec::new();
        let mut y_events = Vec::new();

        for (item_index, item) in items.iter().enumerate() {
            let p0 = item.rect.origin;
            let p1 = item.rect.bottom_right();

            x_events.push(Event::begin(p0.x, item_index));
            x_events.push(Event::end(p1.x, item_index));
            y_events.push(Event::begin(p0.y, item_index));
            y_events.push(Event::end(p1.y, item_index));
        }

        // Get the minimal bounding rect in app units. We will
        // work in fixed point in order to avoid float precision
        // error while handling events.
        let p0 = LayerPointAu::new(
            Au::from_f32_px(bounding_rect.origin.x),
            Au::from_f32_px(bounding_rect.origin.y),
        );

        let p1 = LayerPointAu::new(
            Au::from_f32_px(bounding_rect.origin.x + bounding_rect.size.width),
            Au::from_f32_px(bounding_rect.origin.y + bounding_rect.size.height),
        );

        // Sort the events in ascending order.
        x_events.sort();
        y_events.sort();

        // Generate segments from the event lists, by sweeping the y-axis
        // and then the x-axis for each event. This can generate a significant
        // number of segments, but most importantly, it ensures that there are
        // no t-junctions in the generated segments. It's probably possible
        // to come up with more efficient segmentation algorithms, at least
        // for simple / common cases.

        // Each coordinate is clamped to the bounds of the minimal
        // bounding rect. This ensures that we don't generate segments
        // outside that bounding rect, but does allow correctly handling
        // clips where the clip region starts outside the minimal
        // rect but still intersects with it.

        let mut prev_y = clamp(p0.y, y_events[0].value, p1.y);

        for ey in &y_events {
            let cur_y = clamp(p0.y, ey.value, p1.y);

            if cur_y != prev_y {
                let mut prev_x = clamp(p0.x, x_events[0].value, p1.x);

                for ex in &x_events {
                    let cur_x = clamp(p0.x, ex.value, p1.x);

                    if cur_x != prev_x {
                        if let Some(segment) = emit_segment_if_needed(
                            prev_x,
                            prev_y,
                            cur_x,
                            cur_y,
                            &items,
                            &p0,
                            &p1,
                        ) {
                            f(segment);
                        }

                        prev_x = cur_x;
                    }

                    items[ex.item_index.0].active_x = ex.is_active();
                }

                prev_y = cur_y;
            }

            items[ey.item_index.0].active_y = ey.is_active();
        }
    }
}

fn clamp(low: Au, value: Au, high: Au) -> Au {
    value.max(low).min(high)
}

fn emit_segment_if_needed(
    x0: Au,
    y0: Au,
    x1: Au,
    y1: Au,
    items: &[Item],
    bounds_p0: &LayerPointAu,
    bounds_p1: &LayerPointAu,
) -> Option<Segment> {
    debug_assert!(x1 > x0);
    debug_assert!(y1 > y0);

    // TODO(gw): Don't scan the whole list of items for
    //           each segment rect. Store active list
    //           in a hash set or similar if this ever
    //           shows up in a profile.
    let mut has_clip_mask = false;

    for item in items {
        if item.active_x && item.active_y {
            has_clip_mask |= item.has_mask;

            if item.mode == ClipMode::ClipOut && !item.has_mask {
                return None;
            }
        }
    }

    let segment_rect = LayerRect::new(
        LayerPoint::new(
            x0.to_f32_px(),
            y0.to_f32_px(),
        ),
        LayerSize::new(
            (x1 - x0).to_f32_px(),
            (y1 - y0).to_f32_px(),
        ),
    );

    // Determine which edges touch the bounding rect. This allows
    // the shaders to apply AA correctly along those edges. It also
    // allows the batching code to determine which are inner segments
    // without edges, and push those through the opaque pass.
    let mut edge_flags = EdgeAaSegmentMask::empty();
    if x0 == bounds_p0.x {
        edge_flags |= EdgeAaSegmentMask::LEFT;
    }
    if x1 == bounds_p1.x {
        edge_flags |= EdgeAaSegmentMask::RIGHT;
    }
    if y0 == bounds_p0.y {
        edge_flags |= EdgeAaSegmentMask::TOP;
    }
    if y1 == bounds_p1.y {
        edge_flags |= EdgeAaSegmentMask::BOTTOM;
    }

    Some(Segment {
        rect: segment_rect,
        has_mask: has_clip_mask,
        edge_flags,
    })
}

#[cfg(test)]
mod test {
    use api::{BorderRadius, ClipMode, LayerPoint, LayerRect, LayerSize};
    use prim_store::EdgeAaSegmentMask;
    use super::{Segment, SegmentBuilder};
    use std::cmp;

    fn rect(x0: f32, y0: f32, x1: f32, y1: f32) -> LayerRect {
        LayerRect::new(
            LayerPoint::new(x0, y0),
            LayerSize::new(x1-x0, y1-y0),
        )
    }

    fn seg(
        x0: f32,
        y0: f32,
        x1: f32,
        y1: f32,
        has_mask: bool,
        edge_flags: Option<EdgeAaSegmentMask>,
    ) -> Segment {
        Segment {
            rect: LayerRect::new(
                LayerPoint::new(x0, y0),
                LayerSize::new(x1-x0, y1-y0),
            ),
            has_mask,
            edge_flags: edge_flags.unwrap_or(EdgeAaSegmentMask::empty()),
        }
    }

    fn segment_sorter(s0: &Segment, s1: &Segment) -> cmp::Ordering {
        let r0 = &s0.rect;
        let r1 = &s1.rect;

        (
            (r0.origin.x, r0.origin.y, r0.size.width, r0.size.height)
        ).partial_cmp(&
            (r1.origin.x, r1.origin.y, r1.size.width, r1.size.height)
        ).unwrap()
    }

    fn seg_test(
        local_rect: LayerRect,
        local_clip_rect: LayerRect,
        clips: &[(LayerRect, Option<BorderRadius>, ClipMode)],
        expected_segments: &mut [Segment]
    ) {
        let mut sb = SegmentBuilder::new(
            local_rect,
            local_clip_rect,
        );
        let mut segments = Vec::new();
        for &(rect, radius, mode) in clips {
            sb.push_rect(rect, radius, mode);
        }
        sb.build(|rect| {
            segments.push(rect);
        });
        segments.sort_by(segment_sorter);
        expected_segments.sort_by(segment_sorter);
        assert_eq!(
            segments.len(),
            expected_segments.len(),
            "segments\n{:?}\nexpected\n{:?}\n",
            segments,
            expected_segments
        );
        for (segment, expected) in segments.iter().zip(expected_segments.iter()) {
            assert_eq!(segment, expected);
        }
    }

    #[test]
    fn segment_empty() {
        seg_test(
            rect(0.0, 0.0, 0.0, 0.0),
            rect(0.0, 0.0, 0.0, 0.0),
            &[],
            &mut [],
        );
    }

    #[test]
    fn segment_single() {
        seg_test(
            rect(10.0, 20.0, 30.0, 40.0),
            rect(10.0, 20.0, 30.0, 40.0),
            &[],
            &mut [
                seg(10.0, 20.0, 30.0, 40.0, false,
                    Some(EdgeAaSegmentMask::LEFT |
                         EdgeAaSegmentMask::TOP |
                         EdgeAaSegmentMask::RIGHT |
                         EdgeAaSegmentMask::BOTTOM
                    )
                ),
            ],
        );
    }

    #[test]
    fn segment_single_clip() {
        seg_test(
            rect(10.0, 20.0, 30.0, 40.0),
            rect(10.0, 20.0, 25.0, 35.0),
            &[],
            &mut [
                seg(10.0, 20.0, 25.0, 35.0, false,
                    Some(EdgeAaSegmentMask::LEFT |
                         EdgeAaSegmentMask::TOP |
                         EdgeAaSegmentMask::RIGHT |
                         EdgeAaSegmentMask::BOTTOM
                    )
                ),
            ],
        );
    }

    #[test]
    fn segment_inner_clip() {
        seg_test(
            rect(10.0, 20.0, 30.0, 40.0),
            rect(15.0, 25.0, 25.0, 35.0),
            &[],
            &mut [
                seg(15.0, 25.0, 25.0, 35.0, false,
                    Some(EdgeAaSegmentMask::LEFT |
                         EdgeAaSegmentMask::TOP |
                         EdgeAaSegmentMask::RIGHT |
                         EdgeAaSegmentMask::BOTTOM
                    )
                ),
            ],
        );
    }

    #[test]
    fn segment_outer_clip() {
        seg_test(
            rect(15.0, 25.0, 25.0, 35.0),
            rect(10.0, 20.0, 30.0, 40.0),
            &[],
            &mut [
                seg(15.0, 25.0, 25.0, 35.0, false,
                    Some(EdgeAaSegmentMask::LEFT |
                         EdgeAaSegmentMask::TOP |
                         EdgeAaSegmentMask::RIGHT |
                         EdgeAaSegmentMask::BOTTOM
                    )
                ),
            ],
        );
    }

    #[test]
    fn segment_clip_int() {
        seg_test(
            rect(10.0, 20.0, 30.0, 40.0),
            rect(20.0, 10.0, 40.0, 30.0),
            &[],
            &mut [
                seg(20.0, 20.0, 30.0, 30.0, false,
                    Some(EdgeAaSegmentMask::LEFT |
                         EdgeAaSegmentMask::TOP |
                         EdgeAaSegmentMask::RIGHT |
                         EdgeAaSegmentMask::BOTTOM
                    )
                ),
            ],
        );
    }

    #[test]
    fn segment_clip_disjoint() {
        seg_test(
            rect(10.0, 20.0, 30.0, 40.0),
            rect(30.0, 20.0, 50.0, 40.0),
            &[],
            &mut [],
        );
    }

    #[test]
    fn segment_clips() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(-1000.0, -1000.0, 1000.0, 1000.0),
            &[
                (rect(20.0, 20.0, 40.0, 40.0), None, ClipMode::Clip),
                (rect(40.0, 20.0, 60.0, 40.0), None, ClipMode::Clip),
            ],
            &mut [
            ],
        );
    }

    #[test]
    fn segment_rounded_clip() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(-1000.0, -1000.0, 1000.0, 1000.0),
            &[
                (rect(20.0, 20.0, 60.0, 60.0), Some(BorderRadius::uniform(10.0)), ClipMode::Clip),
            ],
            &mut [
                // corners
                seg(20.0, 20.0, 30.0, 30.0, true, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::TOP)),
                seg(20.0, 50.0, 30.0, 60.0, true, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::BOTTOM)),
                seg(50.0, 20.0, 60.0, 30.0, true, Some(EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::TOP)),
                seg(50.0, 50.0, 60.0, 60.0, true, Some(EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::BOTTOM)),

                // inner
                seg(30.0, 30.0, 50.0, 50.0, false, None),

                // edges
                seg(30.0, 20.0, 50.0, 30.0, false, Some(EdgeAaSegmentMask::TOP)),
                seg(30.0, 50.0, 50.0, 60.0, false, Some(EdgeAaSegmentMask::BOTTOM)),
                seg(20.0, 30.0, 30.0, 50.0, false, Some(EdgeAaSegmentMask::LEFT)),
                seg(50.0, 30.0, 60.0, 50.0, false, Some(EdgeAaSegmentMask::RIGHT)),
            ],
        );
    }

    #[test]
    fn segment_clip_out() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(-1000.0, -1000.0, 2000.0, 2000.0),
            &[
                (rect(20.0, 20.0, 60.0, 60.0), None, ClipMode::ClipOut),
            ],
            &mut [
                seg(0.0, 0.0, 20.0, 20.0, false, Some(EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::LEFT)),
                seg(20.0, 0.0, 60.0, 20.0, false, Some(EdgeAaSegmentMask::TOP)),
                seg(60.0, 0.0, 100.0, 20.0, false, Some(EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::RIGHT)),

                seg(0.0, 20.0, 20.0, 60.0, false, Some(EdgeAaSegmentMask::LEFT)),
                seg(60.0, 20.0, 100.0, 60.0, false, Some(EdgeAaSegmentMask::RIGHT)),

                seg(0.0, 60.0, 20.0, 100.0, false, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::BOTTOM)),
                seg(20.0, 60.0, 60.0, 100.0, false, Some(EdgeAaSegmentMask::BOTTOM)),
                seg(60.0, 60.0, 100.0, 100.0, false, Some(EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::RIGHT)),
            ],
        );
    }

    #[test]
    fn segment_rounded_clip_out() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(-1000.0, -1000.0, 2000.0, 2000.0),
            &[
                (rect(20.0, 20.0, 60.0, 60.0), Some(BorderRadius::uniform(10.0)), ClipMode::ClipOut),
            ],
            &mut [
                // top row
                seg(0.0, 0.0, 20.0, 20.0, false, Some(EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::LEFT)),
                seg(20.0, 0.0, 30.0, 20.0, false, Some(EdgeAaSegmentMask::TOP)),
                seg(30.0, 0.0, 50.0, 20.0, false, Some(EdgeAaSegmentMask::TOP)),
                seg(50.0, 0.0, 60.0, 20.0, false, Some(EdgeAaSegmentMask::TOP)),
                seg(60.0, 0.0, 100.0, 20.0, false, Some(EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::RIGHT)),

                // left
                seg(0.0, 20.0, 20.0, 30.0, false, Some(EdgeAaSegmentMask::LEFT)),
                seg(0.0, 30.0, 20.0, 50.0, false, Some(EdgeAaSegmentMask::LEFT)),
                seg(0.0, 50.0, 20.0, 60.0, false, Some(EdgeAaSegmentMask::LEFT)),

                // right
                seg(60.0, 20.0, 100.0, 30.0, false, Some(EdgeAaSegmentMask::RIGHT)),
                seg(60.0, 30.0, 100.0, 50.0, false, Some(EdgeAaSegmentMask::RIGHT)),
                seg(60.0, 50.0, 100.0, 60.0, false, Some(EdgeAaSegmentMask::RIGHT)),

                // bottom row
                seg(0.0, 60.0, 20.0, 100.0, false, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::BOTTOM)),
                seg(20.0, 60.0, 30.0, 100.0, false, Some(EdgeAaSegmentMask::BOTTOM)),
                seg(30.0, 60.0, 50.0, 100.0, false, Some(EdgeAaSegmentMask::BOTTOM)),
                seg(50.0, 60.0, 60.0, 100.0, false, Some(EdgeAaSegmentMask::BOTTOM)),
                seg(60.0, 60.0, 100.0, 100.0, false, Some(EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::BOTTOM)),

                // inner corners
                seg(20.0, 20.0, 30.0, 30.0, true, None),
                seg(20.0, 50.0, 30.0, 60.0, true, None),
                seg(50.0, 20.0, 60.0, 30.0, true, None),
                seg(50.0, 50.0, 60.0, 60.0, true, None),
            ],
        );
    }

    #[test]
    fn segment_clip_in_clip_out() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(-1000.0, -1000.0, 2000.0, 2000.0),
            &[
                (rect(20.0, 20.0, 60.0, 60.0), None, ClipMode::Clip),
                (rect(50.0, 50.0, 80.0, 80.0), None, ClipMode::ClipOut),
            ],
            &mut [
                seg(20.0, 20.0, 50.0, 50.0, false, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::TOP)),
                seg(50.0, 20.0, 60.0, 50.0, false, Some(EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::RIGHT)),
                seg(20.0, 50.0, 50.0, 60.0, false, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::BOTTOM)),
            ],
        );
    }

    #[test]
    fn segment_rounded_clip_overlap() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(0.0, 0.0, 100.0, 100.0),
            &[
                (rect(0.0, 0.0, 10.0, 10.0), None, ClipMode::ClipOut),
                (rect(0.0, 0.0, 100.0, 100.0), Some(BorderRadius::uniform(10.0)), ClipMode::Clip),
            ],
            &mut [
                // corners
                seg(0.0, 90.0, 10.0, 100.0, true, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::BOTTOM)),
                seg(90.0, 0.0, 100.0, 10.0, true, Some(EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::TOP)),
                seg(90.0, 90.0, 100.0, 100.0, true, Some(EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::BOTTOM)),

                // inner
                seg(10.0, 10.0, 90.0, 90.0, false, None),

                // edges
                seg(10.0, 0.0, 90.0, 10.0, false, Some(EdgeAaSegmentMask::TOP)),
                seg(10.0, 90.0, 90.0, 100.0, false, Some(EdgeAaSegmentMask::BOTTOM)),
                seg(0.0, 10.0, 10.0, 90.0, false, Some(EdgeAaSegmentMask::LEFT)),
                seg(90.0, 10.0, 100.0, 90.0, false, Some(EdgeAaSegmentMask::RIGHT)),
            ],
        );
    }

    #[test]
    fn segment_rounded_clip_overlap_reverse() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(0.0, 0.0, 100.0, 100.0),
            &[
                (rect(10.0, 10.0, 90.0, 90.0), None, ClipMode::Clip),
                (rect(0.0, 0.0, 100.0, 100.0), Some(BorderRadius::uniform(10.0)), ClipMode::Clip),
            ],
            &mut [
                seg(10.0, 10.0, 90.0, 90.0, false,
                    Some(EdgeAaSegmentMask::LEFT |
                         EdgeAaSegmentMask::TOP |
                         EdgeAaSegmentMask::RIGHT |
                         EdgeAaSegmentMask::BOTTOM
                    )
                ),
            ],
        );
    }

    #[test]
    fn segment_clip_in_clip_out_overlap() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(0.0, 0.0, 100.0, 100.0),
            &[
                (rect(10.0, 10.0, 90.0, 90.0), None, ClipMode::Clip),
                (rect(10.0, 10.0, 90.0, 90.0), None, ClipMode::ClipOut),
            ],
            &mut [
            ],
        );
    }

    #[test]
    fn segment_event_order() {
        seg_test(
            rect(0.0, 0.0, 100.0, 100.0),
            rect(0.0, 0.0, 100.0, 100.0),
            &[
                (rect(0.0, 0.0, 100.0, 90.0), None, ClipMode::ClipOut),
            ],
            &mut [
                seg(0.0, 90.0, 100.0, 100.0, false, Some(EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::BOTTOM)),
            ],
        );
    }
}
