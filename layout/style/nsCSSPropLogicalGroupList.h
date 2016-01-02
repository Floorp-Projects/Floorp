/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of groups of logical properties, for preprocessing
 */

// A logical property group is one that defines the corresponding physical
// longhand properties that could be set by a given set of logical longhand
// properties.  For example, the logical property group for margin-block-start
// (and the other three logical margin properties) is one that contains
// margin-top, margin-right, margin-bottom and margin-left.
//
// Logical property groups are defined below using one of the following
// macros, where the name_ argument must be capitalized LikeThis and
// must not collide with the name of a property's DOM method (its
// method_ in nsCSSPropList.h):
//
//   CSS_PROP_LOGICAL_GROUP_SHORTHAND(name_)
//     Defines a logical property group whose corresponding physical
//     properties are those in a given shorthand.  For example, the
//     logical property group for margin-{block,inline}-{start,end}
//     is defined by the margin shorthand.  The name_ argument must
//     be the method_ name of the shorthand (so Margin rather than
//     margin).
//
//   CSS_PROP_LOGICAL_GROUP_BOX(name_)
//     Defines a logical property group whose corresponding physical
//     properties are a set of four box properties which are not
//     already represented by an existing shorthand property.  For
//     example, the logical property group for
//     offset-{block,inline}-{start,end} contains the top, right,
//     bottom and left physical properties, but there is no shorthand
//     that sets those four properties.  A table must be defined in
//     nsCSSProps.cpp named g<name_>LogicalGroupTable containing the
//     four physical properties in top/right/bottom/left order.
//
//   CSS_PROP_LOGICAL_GROUP_AXIS(name_)
//     Defines a logical property group whose corresponding physical
//     properties are a set of two axis-related properties.  For
//     example, the logical property group for {block,inline}-size
//     contains the width and height properties.  A table must be
//     defined in nCSSProps.cpp named g<name_>LogicalGroupTable
//     containing the two physical properties in vertical/horizontal
//     order, followed by an nsCSSProperty_UNKNOWN entry.
//
//   CSS_PROP_LOGICAL_GROUP_SINGLE(name_)
//     Defines a logical property group in which the logical property always
//     maps to the same physical property. For such properties, the
//     "logicalness" is in the value-mapping, not in the property-mapping.  For
//     example, the logical property "-webkit-box-orient" is always mapped to
//     "flex-direction", but its values ("horizontal", "vertical") map to
//     different flex-direction values ("row", "column") depending on the
//     writing-mode.  A table must be defined in nsCSSProps.cpp named
//     g<name_>LogicalGroupTable containing the one physical property,
//     followed by an nsCSSProperty_UNKNOWN entry.

CSS_PROP_LOGICAL_GROUP_SHORTHAND(BorderColor)
CSS_PROP_LOGICAL_GROUP_SHORTHAND(BorderStyle)
CSS_PROP_LOGICAL_GROUP_SHORTHAND(BorderWidth)
CSS_PROP_LOGICAL_GROUP_SHORTHAND(Margin)
CSS_PROP_LOGICAL_GROUP_AXIS(MaxSize)
CSS_PROP_LOGICAL_GROUP_BOX(Offset)
CSS_PROP_LOGICAL_GROUP_SHORTHAND(Padding)
CSS_PROP_LOGICAL_GROUP_AXIS(MinSize)
CSS_PROP_LOGICAL_GROUP_AXIS(Size)
CSS_PROP_LOGICAL_GROUP_SINGLE(WebkitBoxOrient)
