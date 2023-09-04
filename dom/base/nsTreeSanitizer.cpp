/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeSanitizer.h"

#include "mozilla/Algorithm.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/SanitizerBinding.h"
#include "mozilla/dom/ShadowIncludingTreeIterator.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/NullPrincipal.h"
#include "nsAtom.h"
#include "nsCSSPropertyID.h"
#include "nsHashtablesFwd.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsUnicharInputStream.h"
#include "nsAttrName.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIParserUtils.h"
#include "mozilla/dom/Document.h"
#include "nsQueryObject.h"

#include <iterator>

using namespace mozilla;
using namespace mozilla::dom;

//
// Thanks to Mark Pilgrim and Sam Ruby for the initial whitelist
//
const nsStaticAtom* const kElementsHTML[] = {
    // clang-format off
  nsGkAtoms::a,
  nsGkAtoms::abbr,
  nsGkAtoms::acronym,
  nsGkAtoms::address,
  nsGkAtoms::area,
  nsGkAtoms::article,
  nsGkAtoms::aside,
  nsGkAtoms::audio,
  nsGkAtoms::b,
  nsGkAtoms::bdi,
  nsGkAtoms::bdo,
  nsGkAtoms::big,
  nsGkAtoms::blockquote,
  // body checked specially
  nsGkAtoms::br,
  nsGkAtoms::button,
  nsGkAtoms::canvas,
  nsGkAtoms::caption,
  nsGkAtoms::center,
  nsGkAtoms::cite,
  nsGkAtoms::code,
  nsGkAtoms::col,
  nsGkAtoms::colgroup,
  nsGkAtoms::data,
  nsGkAtoms::datalist,
  nsGkAtoms::dd,
  nsGkAtoms::del,
  nsGkAtoms::details,
  nsGkAtoms::dfn,
  nsGkAtoms::dialog,
  nsGkAtoms::dir,
  nsGkAtoms::div,
  nsGkAtoms::dl,
  nsGkAtoms::dt,
  nsGkAtoms::em,
  nsGkAtoms::fieldset,
  nsGkAtoms::figcaption,
  nsGkAtoms::figure,
  nsGkAtoms::font,
  nsGkAtoms::footer,
  nsGkAtoms::form,
  nsGkAtoms::h1,
  nsGkAtoms::h2,
  nsGkAtoms::h3,
  nsGkAtoms::h4,
  nsGkAtoms::h5,
  nsGkAtoms::h6,
  // head checked specially
  nsGkAtoms::header,
  nsGkAtoms::hgroup,
  nsGkAtoms::hr,
  // html checked specially
  nsGkAtoms::i,
  nsGkAtoms::img,
  nsGkAtoms::input,
  nsGkAtoms::ins,
  nsGkAtoms::kbd,
  nsGkAtoms::keygen,
  nsGkAtoms::label,
  nsGkAtoms::legend,
  nsGkAtoms::li,
  nsGkAtoms::link,
  nsGkAtoms::listing,
  nsGkAtoms::main,
  nsGkAtoms::map,
  nsGkAtoms::mark,
  nsGkAtoms::menu,
  nsGkAtoms::meta,
  nsGkAtoms::meter,
  nsGkAtoms::nav,
  nsGkAtoms::nobr,
  nsGkAtoms::noscript,
  nsGkAtoms::ol,
  nsGkAtoms::optgroup,
  nsGkAtoms::option,
  nsGkAtoms::output,
  nsGkAtoms::p,
  nsGkAtoms::picture,
  nsGkAtoms::pre,
  nsGkAtoms::progress,
  nsGkAtoms::q,
  nsGkAtoms::rb,
  nsGkAtoms::rp,
  nsGkAtoms::rt,
  nsGkAtoms::rtc,
  nsGkAtoms::ruby,
  nsGkAtoms::s,
  nsGkAtoms::samp,
  nsGkAtoms::section,
  nsGkAtoms::select,
  nsGkAtoms::small,
  nsGkAtoms::source,
  nsGkAtoms::span,
  nsGkAtoms::strike,
  nsGkAtoms::strong,
  nsGkAtoms::sub,
  nsGkAtoms::summary,
  nsGkAtoms::sup,
  // style checked specially
  nsGkAtoms::table,
  nsGkAtoms::tbody,
  nsGkAtoms::td,
  // template checked and traversed specially
  nsGkAtoms::textarea,
  nsGkAtoms::tfoot,
  nsGkAtoms::th,
  nsGkAtoms::thead,
  nsGkAtoms::time,
  // title checked specially
  nsGkAtoms::tr,
  nsGkAtoms::track,
  nsGkAtoms::tt,
  nsGkAtoms::u,
  nsGkAtoms::ul,
  nsGkAtoms::var,
  nsGkAtoms::video,
  nsGkAtoms::wbr,
  nullptr
    // clang-format on
};

const nsStaticAtom* const kAttributesHTML[] = {
    // clang-format off
  nsGkAtoms::abbr,
  nsGkAtoms::accept,
  nsGkAtoms::acceptcharset,
  nsGkAtoms::accesskey,
  nsGkAtoms::action,
  nsGkAtoms::alt,
  nsGkAtoms::as,
  nsGkAtoms::autocomplete,
  nsGkAtoms::autofocus,
  nsGkAtoms::autoplay,
  nsGkAtoms::axis,
  nsGkAtoms::_char,
  nsGkAtoms::charoff,
  nsGkAtoms::charset,
  nsGkAtoms::checked,
  nsGkAtoms::cite,
  nsGkAtoms::_class,
  nsGkAtoms::cols,
  nsGkAtoms::colspan,
  nsGkAtoms::content,
  nsGkAtoms::contenteditable,
  nsGkAtoms::contextmenu,
  nsGkAtoms::controls,
  nsGkAtoms::coords,
  nsGkAtoms::crossorigin,
  nsGkAtoms::datetime,
  nsGkAtoms::dir,
  nsGkAtoms::disabled,
  nsGkAtoms::draggable,
  nsGkAtoms::enctype,
  nsGkAtoms::face,
  nsGkAtoms::_for,
  nsGkAtoms::frame,
  nsGkAtoms::headers,
  nsGkAtoms::height,
  nsGkAtoms::hidden,
  nsGkAtoms::high,
  nsGkAtoms::href,
  nsGkAtoms::hreflang,
  nsGkAtoms::icon,
  nsGkAtoms::id,
  nsGkAtoms::integrity,
  nsGkAtoms::ismap,
  nsGkAtoms::itemid,
  nsGkAtoms::itemprop,
  nsGkAtoms::itemref,
  nsGkAtoms::itemscope,
  nsGkAtoms::itemtype,
  nsGkAtoms::kind,
  nsGkAtoms::label,
  nsGkAtoms::lang,
  nsGkAtoms::list_,
  nsGkAtoms::longdesc,
  nsGkAtoms::loop,
  nsGkAtoms::low,
  nsGkAtoms::max,
  nsGkAtoms::maxlength,
  nsGkAtoms::media,
  nsGkAtoms::method,
  nsGkAtoms::min,
  nsGkAtoms::minlength,
  nsGkAtoms::multiple,
  nsGkAtoms::muted,
  nsGkAtoms::name,
  nsGkAtoms::nohref,
  nsGkAtoms::novalidate,
  nsGkAtoms::nowrap,
  nsGkAtoms::open,
  nsGkAtoms::optimum,
  nsGkAtoms::pattern,
  nsGkAtoms::placeholder,
  nsGkAtoms::playbackrate,
  nsGkAtoms::poster,
  nsGkAtoms::preload,
  nsGkAtoms::prompt,
  nsGkAtoms::pubdate,
  nsGkAtoms::radiogroup,
  nsGkAtoms::readonly,
  nsGkAtoms::rel,
  nsGkAtoms::required,
  nsGkAtoms::rev,
  nsGkAtoms::reversed,
  nsGkAtoms::role,
  nsGkAtoms::rows,
  nsGkAtoms::rowspan,
  nsGkAtoms::rules,
  nsGkAtoms::scoped,
  nsGkAtoms::scope,
  nsGkAtoms::selected,
  nsGkAtoms::shape,
  nsGkAtoms::span,
  nsGkAtoms::spellcheck,
  nsGkAtoms::src,
  nsGkAtoms::srclang,
  nsGkAtoms::start,
  nsGkAtoms::summary,
  nsGkAtoms::tabindex,
  nsGkAtoms::target,
  nsGkAtoms::title,
  nsGkAtoms::type,
  nsGkAtoms::usemap,
  nsGkAtoms::value,
  nsGkAtoms::width,
  nsGkAtoms::wrap,
  nullptr
    // clang-format on
};

const nsStaticAtom* const kPresAttributesHTML[] = {
    // clang-format off
  nsGkAtoms::align,
  nsGkAtoms::background,
  nsGkAtoms::bgcolor,
  nsGkAtoms::border,
  nsGkAtoms::cellpadding,
  nsGkAtoms::cellspacing,
  nsGkAtoms::color,
  nsGkAtoms::compact,
  nsGkAtoms::clear,
  nsGkAtoms::hspace,
  nsGkAtoms::noshade,
  nsGkAtoms::pointSize,
  nsGkAtoms::size,
  nsGkAtoms::valign,
  nsGkAtoms::vspace,
  nullptr
    // clang-format on
};

// List of HTML attributes with URLs that the
// browser will fetch. Should be kept in sync with
// https://html.spec.whatwg.org/multipage/indices.html#attributes-3
const nsStaticAtom* const kURLAttributesHTML[] = {
    // clang-format off
  nsGkAtoms::action,
  nsGkAtoms::href,
  nsGkAtoms::src,
  nsGkAtoms::longdesc,
  nsGkAtoms::cite,
  nsGkAtoms::background,
  nsGkAtoms::formaction,
  nsGkAtoms::data,
  nsGkAtoms::ping,
  nsGkAtoms::poster,
  nullptr
    // clang-format on
};

const nsStaticAtom* const kElementsSVG[] = {
    nsGkAtoms::a,                    // a
    nsGkAtoms::circle,               // circle
    nsGkAtoms::clipPath,             // clipPath
    nsGkAtoms::colorProfile,         // color-profile
    nsGkAtoms::cursor,               // cursor
    nsGkAtoms::defs,                 // defs
    nsGkAtoms::desc,                 // desc
    nsGkAtoms::ellipse,              // ellipse
    nsGkAtoms::elevation,            // elevation
    nsGkAtoms::erode,                // erode
    nsGkAtoms::ex,                   // ex
    nsGkAtoms::exact,                // exact
    nsGkAtoms::exponent,             // exponent
    nsGkAtoms::feBlend,              // feBlend
    nsGkAtoms::feColorMatrix,        // feColorMatrix
    nsGkAtoms::feComponentTransfer,  // feComponentTransfer
    nsGkAtoms::feComposite,          // feComposite
    nsGkAtoms::feConvolveMatrix,     // feConvolveMatrix
    nsGkAtoms::feDiffuseLighting,    // feDiffuseLighting
    nsGkAtoms::feDisplacementMap,    // feDisplacementMap
    nsGkAtoms::feDistantLight,       // feDistantLight
    nsGkAtoms::feDropShadow,         // feDropShadow
    nsGkAtoms::feFlood,              // feFlood
    nsGkAtoms::feFuncA,              // feFuncA
    nsGkAtoms::feFuncB,              // feFuncB
    nsGkAtoms::feFuncG,              // feFuncG
    nsGkAtoms::feFuncR,              // feFuncR
    nsGkAtoms::feGaussianBlur,       // feGaussianBlur
    nsGkAtoms::feImage,              // feImage
    nsGkAtoms::feMerge,              // feMerge
    nsGkAtoms::feMergeNode,          // feMergeNode
    nsGkAtoms::feMorphology,         // feMorphology
    nsGkAtoms::feOffset,             // feOffset
    nsGkAtoms::fePointLight,         // fePointLight
    nsGkAtoms::feSpecularLighting,   // feSpecularLighting
    nsGkAtoms::feSpotLight,          // feSpotLight
    nsGkAtoms::feTile,               // feTile
    nsGkAtoms::feTurbulence,         // feTurbulence
    nsGkAtoms::filter,               // filter
    nsGkAtoms::font,                 // font
    nsGkAtoms::font_face,            // font-face
    nsGkAtoms::font_face_format,     // font-face-format
    nsGkAtoms::font_face_name,       // font-face-name
    nsGkAtoms::font_face_src,        // font-face-src
    nsGkAtoms::font_face_uri,        // font-face-uri
    nsGkAtoms::foreignObject,        // foreignObject
    nsGkAtoms::g,                    // g
    // glyph
    nsGkAtoms::glyphRef,  // glyphRef
    // hkern
    nsGkAtoms::image,           // image
    nsGkAtoms::line,            // line
    nsGkAtoms::linearGradient,  // linearGradient
    nsGkAtoms::marker,          // marker
    nsGkAtoms::mask,            // mask
    nsGkAtoms::metadata,        // metadata
    nsGkAtoms::missingGlyph,    // missingGlyph
    nsGkAtoms::mpath,           // mpath
    nsGkAtoms::path,            // path
    nsGkAtoms::pattern,         // pattern
    nsGkAtoms::polygon,         // polygon
    nsGkAtoms::polyline,        // polyline
    nsGkAtoms::radialGradient,  // radialGradient
    nsGkAtoms::rect,            // rect
    nsGkAtoms::stop,            // stop
    nsGkAtoms::svg,             // svg
    nsGkAtoms::svgSwitch,       // switch
    nsGkAtoms::symbol,          // symbol
    nsGkAtoms::text,            // text
    nsGkAtoms::textPath,        // textPath
    nsGkAtoms::title,           // title
    nsGkAtoms::tref,            // tref
    nsGkAtoms::tspan,           // tspan
    nsGkAtoms::use,             // use
    nsGkAtoms::view,            // view
    // vkern
    nullptr};

constexpr const nsStaticAtom* const kAttributesSVG[] = {
    // accent-height
    nsGkAtoms::accumulate,          // accumulate
    nsGkAtoms::additive,            // additive
    nsGkAtoms::alignment_baseline,  // alignment-baseline
    // alphabetic
    nsGkAtoms::amplitude,  // amplitude
    // arabic-form
    // ascent
    nsGkAtoms::attributeName,   // attributeName
    nsGkAtoms::attributeType,   // attributeType
    nsGkAtoms::azimuth,         // azimuth
    nsGkAtoms::baseFrequency,   // baseFrequency
    nsGkAtoms::baseline_shift,  // baseline-shift
    // baseProfile
    // bbox
    nsGkAtoms::begin,     // begin
    nsGkAtoms::bias,      // bias
    nsGkAtoms::by,        // by
    nsGkAtoms::calcMode,  // calcMode
    // cap-height
    nsGkAtoms::_class,                     // class
    nsGkAtoms::clip_path,                  // clip-path
    nsGkAtoms::clip_rule,                  // clip-rule
    nsGkAtoms::clipPathUnits,              // clipPathUnits
    nsGkAtoms::color,                      // color
    nsGkAtoms::colorInterpolation,         // color-interpolation
    nsGkAtoms::colorInterpolationFilters,  // color-interpolation-filters
    nsGkAtoms::cursor,                     // cursor
    nsGkAtoms::cx,                         // cx
    nsGkAtoms::cy,                         // cy
    nsGkAtoms::d,                          // d
    // descent
    nsGkAtoms::diffuseConstant,    // diffuseConstant
    nsGkAtoms::direction,          // direction
    nsGkAtoms::display,            // display
    nsGkAtoms::divisor,            // divisor
    nsGkAtoms::dominant_baseline,  // dominant-baseline
    nsGkAtoms::dur,                // dur
    nsGkAtoms::dx,                 // dx
    nsGkAtoms::dy,                 // dy
    nsGkAtoms::edgeMode,           // edgeMode
    nsGkAtoms::elevation,          // elevation
    // enable-background
    nsGkAtoms::end,            // end
    nsGkAtoms::fill,           // fill
    nsGkAtoms::fill_opacity,   // fill-opacity
    nsGkAtoms::fill_rule,      // fill-rule
    nsGkAtoms::filter,         // filter
    nsGkAtoms::filterUnits,    // filterUnits
    nsGkAtoms::flood_color,    // flood-color
    nsGkAtoms::flood_opacity,  // flood-opacity
    // XXX focusable
    nsGkAtoms::font,              // font
    nsGkAtoms::font_family,       // font-family
    nsGkAtoms::font_size,         // font-size
    nsGkAtoms::font_size_adjust,  // font-size-adjust
    nsGkAtoms::font_stretch,      // font-stretch
    nsGkAtoms::font_style,        // font-style
    nsGkAtoms::font_variant,      // font-variant
    nsGkAtoms::fontWeight,        // font-weight
    nsGkAtoms::format,            // format
    nsGkAtoms::from,              // from
    nsGkAtoms::fx,                // fx
    nsGkAtoms::fy,                // fy
    // g1
    // g2
    // glyph-name
    // glyphRef
    // glyph-orientation-horizontal
    // glyph-orientation-vertical
    nsGkAtoms::gradientTransform,  // gradientTransform
    nsGkAtoms::gradientUnits,      // gradientUnits
    nsGkAtoms::height,             // height
    nsGkAtoms::href,
    // horiz-adv-x
    // horiz-origin-x
    // horiz-origin-y
    nsGkAtoms::id,  // id
    // ideographic
    nsGkAtoms::image_rendering,  // image-rendering
    nsGkAtoms::in,               // in
    nsGkAtoms::in2,              // in2
    nsGkAtoms::intercept,        // intercept
    // k
    nsGkAtoms::k1,  // k1
    nsGkAtoms::k2,  // k2
    nsGkAtoms::k3,  // k3
    nsGkAtoms::k4,  // k4
    // kerning
    nsGkAtoms::kernelMatrix,      // kernelMatrix
    nsGkAtoms::kernelUnitLength,  // kernelUnitLength
    nsGkAtoms::keyPoints,         // keyPoints
    nsGkAtoms::keySplines,        // keySplines
    nsGkAtoms::keyTimes,          // keyTimes
    nsGkAtoms::lang,              // lang
    // lengthAdjust
    nsGkAtoms::letter_spacing,     // letter-spacing
    nsGkAtoms::lighting_color,     // lighting-color
    nsGkAtoms::limitingConeAngle,  // limitingConeAngle
    // local
    nsGkAtoms::marker,            // marker
    nsGkAtoms::marker_end,        // marker-end
    nsGkAtoms::marker_mid,        // marker-mid
    nsGkAtoms::marker_start,      // marker-start
    nsGkAtoms::markerHeight,      // markerHeight
    nsGkAtoms::markerUnits,       // markerUnits
    nsGkAtoms::markerWidth,       // markerWidth
    nsGkAtoms::mask,              // mask
    nsGkAtoms::maskContentUnits,  // maskContentUnits
    nsGkAtoms::maskUnits,         // maskUnits
    // mathematical
    nsGkAtoms::max,          // max
    nsGkAtoms::media,        // media
    nsGkAtoms::method,       // method
    nsGkAtoms::min,          // min
    nsGkAtoms::mode,         // mode
    nsGkAtoms::name,         // name
    nsGkAtoms::numOctaves,   // numOctaves
    nsGkAtoms::offset,       // offset
    nsGkAtoms::opacity,      // opacity
    nsGkAtoms::_operator,    // operator
    nsGkAtoms::order,        // order
    nsGkAtoms::orient,       // orient
    nsGkAtoms::orientation,  // orientation
    // origin
    // overline-position
    // overline-thickness
    nsGkAtoms::overflow,  // overflow
    // panose-1
    nsGkAtoms::path,                 // path
    nsGkAtoms::pathLength,           // pathLength
    nsGkAtoms::patternContentUnits,  // patternContentUnits
    nsGkAtoms::patternTransform,     // patternTransform
    nsGkAtoms::patternUnits,         // patternUnits
    nsGkAtoms::pointer_events,       // pointer-events XXX is this safe?
    nsGkAtoms::points,               // points
    nsGkAtoms::pointsAtX,            // pointsAtX
    nsGkAtoms::pointsAtY,            // pointsAtY
    nsGkAtoms::pointsAtZ,            // pointsAtZ
    nsGkAtoms::preserveAlpha,        // preserveAlpha
    nsGkAtoms::preserveAspectRatio,  // preserveAspectRatio
    nsGkAtoms::primitiveUnits,       // primitiveUnits
    nsGkAtoms::r,                    // r
    nsGkAtoms::radius,               // radius
    nsGkAtoms::refX,                 // refX
    nsGkAtoms::refY,                 // refY
    nsGkAtoms::repeatCount,          // repeatCount
    nsGkAtoms::repeatDur,            // repeatDur
    nsGkAtoms::requiredExtensions,   // requiredExtensions
    nsGkAtoms::requiredFeatures,     // requiredFeatures
    nsGkAtoms::restart,              // restart
    nsGkAtoms::result,               // result
    nsGkAtoms::rotate,               // rotate
    nsGkAtoms::rx,                   // rx
    nsGkAtoms::ry,                   // ry
    nsGkAtoms::scale,                // scale
    nsGkAtoms::seed,                 // seed
    nsGkAtoms::shape_rendering,      // shape-rendering
    nsGkAtoms::slope,                // slope
    nsGkAtoms::spacing,              // spacing
    nsGkAtoms::specularConstant,     // specularConstant
    nsGkAtoms::specularExponent,     // specularExponent
    nsGkAtoms::spreadMethod,         // spreadMethod
    nsGkAtoms::startOffset,          // startOffset
    nsGkAtoms::stdDeviation,         // stdDeviation
    // stemh
    // stemv
    nsGkAtoms::stitchTiles,   // stitchTiles
    nsGkAtoms::stop_color,    // stop-color
    nsGkAtoms::stop_opacity,  // stop-opacity
    // strikethrough-position
    // strikethrough-thickness
    nsGkAtoms::string,             // string
    nsGkAtoms::stroke,             // stroke
    nsGkAtoms::stroke_dasharray,   // stroke-dasharray
    nsGkAtoms::stroke_dashoffset,  // stroke-dashoffset
    nsGkAtoms::stroke_linecap,     // stroke-linecap
    nsGkAtoms::stroke_linejoin,    // stroke-linejoin
    nsGkAtoms::stroke_miterlimit,  // stroke-miterlimit
    nsGkAtoms::stroke_opacity,     // stroke-opacity
    nsGkAtoms::stroke_width,       // stroke-width
    nsGkAtoms::surfaceScale,       // surfaceScale
    nsGkAtoms::systemLanguage,     // systemLanguage
    nsGkAtoms::tableValues,        // tableValues
    nsGkAtoms::target,             // target
    nsGkAtoms::targetX,            // targetX
    nsGkAtoms::targetY,            // targetY
    nsGkAtoms::text_anchor,        // text-anchor
    nsGkAtoms::text_decoration,    // text-decoration
    // textLength
    nsGkAtoms::text_rendering,    // text-rendering
    nsGkAtoms::title,             // title
    nsGkAtoms::to,                // to
    nsGkAtoms::transform,         // transform
    nsGkAtoms::transform_origin,  // transform-origin
    nsGkAtoms::type,              // type
    // u1
    // u2
    // underline-position
    // underline-thickness
    // unicode
    nsGkAtoms::unicode_bidi,  // unicode-bidi
    // unicode-range
    // units-per-em
    // v-alphabetic
    // v-hanging
    // v-ideographic
    // v-mathematical
    nsGkAtoms::values,         // values
    nsGkAtoms::vector_effect,  // vector-effect
    // vert-adv-y
    // vert-origin-x
    // vert-origin-y
    nsGkAtoms::viewBox,     // viewBox
    nsGkAtoms::viewTarget,  // viewTarget
    nsGkAtoms::visibility,  // visibility
    nsGkAtoms::width,       // width
    // widths
    nsGkAtoms::word_spacing,  // word-spacing
    nsGkAtoms::writing_mode,  // writing-mode
    nsGkAtoms::x,             // x
    // x-height
    nsGkAtoms::x1,                // x1
    nsGkAtoms::x2,                // x2
    nsGkAtoms::xChannelSelector,  // xChannelSelector
    nsGkAtoms::y,                 // y
    nsGkAtoms::y1,                // y1
    nsGkAtoms::y2,                // y2
    nsGkAtoms::yChannelSelector,  // yChannelSelector
    nsGkAtoms::z,                 // z
    nsGkAtoms::zoomAndPan,        // zoomAndPan
    nullptr};

constexpr const nsStaticAtom* const kURLAttributesSVG[] = {nsGkAtoms::href,
                                                           nullptr};

static_assert(AllOf(std::begin(kURLAttributesSVG), std::end(kURLAttributesSVG),
                    [](auto aURLAttributeSVG) {
                      return AnyOf(std::begin(kAttributesSVG),
                                   std::end(kAttributesSVG),
                                   [&](auto aAttributeSVG) {
                                     return aAttributeSVG == aURLAttributeSVG;
                                   });
                    }));

const nsStaticAtom* const kElementsMathML[] = {
    nsGkAtoms::abs_,                  // abs
    nsGkAtoms::_and,                  // and
    nsGkAtoms::annotation_,           // annotation
    nsGkAtoms::annotation_xml_,       // annotation-xml
    nsGkAtoms::apply_,                // apply
    nsGkAtoms::approx_,               // approx
    nsGkAtoms::arccos_,               // arccos
    nsGkAtoms::arccosh_,              // arccosh
    nsGkAtoms::arccot_,               // arccot
    nsGkAtoms::arccoth_,              // arccoth
    nsGkAtoms::arccsc_,               // arccsc
    nsGkAtoms::arccsch_,              // arccsch
    nsGkAtoms::arcsec_,               // arcsec
    nsGkAtoms::arcsech_,              // arcsech
    nsGkAtoms::arcsin_,               // arcsin
    nsGkAtoms::arcsinh_,              // arcsinh
    nsGkAtoms::arctan_,               // arctan
    nsGkAtoms::arctanh_,              // arctanh
    nsGkAtoms::arg_,                  // arg
    nsGkAtoms::bind_,                 // bind
    nsGkAtoms::bvar_,                 // bvar
    nsGkAtoms::card_,                 // card
    nsGkAtoms::cartesianproduct_,     // cartesianproduct
    nsGkAtoms::cbytes_,               // cbytes
    nsGkAtoms::ceiling,               // ceiling
    nsGkAtoms::cerror_,               // cerror
    nsGkAtoms::ci_,                   // ci
    nsGkAtoms::cn_,                   // cn
    nsGkAtoms::codomain_,             // codomain
    nsGkAtoms::complexes_,            // complexes
    nsGkAtoms::compose_,              // compose
    nsGkAtoms::condition_,            // condition
    nsGkAtoms::conjugate_,            // conjugate
    nsGkAtoms::cos_,                  // cos
    nsGkAtoms::cosh_,                 // cosh
    nsGkAtoms::cot_,                  // cot
    nsGkAtoms::coth_,                 // coth
    nsGkAtoms::cs_,                   // cs
    nsGkAtoms::csc_,                  // csc
    nsGkAtoms::csch_,                 // csch
    nsGkAtoms::csymbol_,              // csymbol
    nsGkAtoms::curl_,                 // curl
    nsGkAtoms::declare,               // declare
    nsGkAtoms::degree_,               // degree
    nsGkAtoms::determinant_,          // determinant
    nsGkAtoms::diff_,                 // diff
    nsGkAtoms::divergence_,           // divergence
    nsGkAtoms::divide_,               // divide
    nsGkAtoms::domain_,               // domain
    nsGkAtoms::domainofapplication_,  // domainofapplication
    nsGkAtoms::el,                    // el
    nsGkAtoms::emptyset_,             // emptyset
    nsGkAtoms::eq_,                   // eq
    nsGkAtoms::equivalent_,           // equivalent
    nsGkAtoms::eulergamma_,           // eulergamma
    nsGkAtoms::exists_,               // exists
    nsGkAtoms::exp_,                  // exp
    nsGkAtoms::exponentiale_,         // exponentiale
    nsGkAtoms::factorial_,            // factorial
    nsGkAtoms::factorof_,             // factorof
    nsGkAtoms::_false,                // false
    nsGkAtoms::floor,                 // floor
    nsGkAtoms::fn_,                   // fn
    nsGkAtoms::forall_,               // forall
    nsGkAtoms::gcd_,                  // gcd
    nsGkAtoms::geq_,                  // geq
    nsGkAtoms::grad,                  // grad
    nsGkAtoms::gt_,                   // gt
    nsGkAtoms::ident_,                // ident
    nsGkAtoms::image,                 // image
    nsGkAtoms::imaginary_,            // imaginary
    nsGkAtoms::imaginaryi_,           // imaginaryi
    nsGkAtoms::implies_,              // implies
    nsGkAtoms::in,                    // in
    nsGkAtoms::infinity,              // infinity
    nsGkAtoms::int_,                  // int
    nsGkAtoms::integers_,             // integers
    nsGkAtoms::intersect_,            // intersect
    nsGkAtoms::interval_,             // interval
    nsGkAtoms::inverse_,              // inverse
    nsGkAtoms::lambda_,               // lambda
    nsGkAtoms::laplacian_,            // laplacian
    nsGkAtoms::lcm_,                  // lcm
    nsGkAtoms::leq_,                  // leq
    nsGkAtoms::limit_,                // limit
    nsGkAtoms::list_,                 // list
    nsGkAtoms::ln_,                   // ln
    nsGkAtoms::log_,                  // log
    nsGkAtoms::logbase_,              // logbase
    nsGkAtoms::lowlimit_,             // lowlimit
    nsGkAtoms::lt_,                   // lt
    nsGkAtoms::maction_,              // maction
    nsGkAtoms::maligngroup_,          // maligngroup
    nsGkAtoms::malignmark_,           // malignmark
    nsGkAtoms::math,                  // math
    nsGkAtoms::matrix,                // matrix
    nsGkAtoms::matrixrow_,            // matrixrow
    nsGkAtoms::max,                   // max
    nsGkAtoms::mean_,                 // mean
    nsGkAtoms::median_,               // median
    nsGkAtoms::menclose_,             // menclose
    nsGkAtoms::merror_,               // merror
    nsGkAtoms::mfrac_,                // mfrac
    nsGkAtoms::mglyph_,               // mglyph
    nsGkAtoms::mi_,                   // mi
    nsGkAtoms::min,                   // min
    nsGkAtoms::minus_,                // minus
    nsGkAtoms::mlabeledtr_,           // mlabeledtr
    nsGkAtoms::mlongdiv_,             // mlongdiv
    nsGkAtoms::mmultiscripts_,        // mmultiscripts
    nsGkAtoms::mn_,                   // mn
    nsGkAtoms::mo_,                   // mo
    nsGkAtoms::mode,                  // mode
    nsGkAtoms::moment_,               // moment
    nsGkAtoms::momentabout_,          // momentabout
    nsGkAtoms::mover_,                // mover
    nsGkAtoms::mpadded_,              // mpadded
    nsGkAtoms::mphantom_,             // mphantom
    nsGkAtoms::mprescripts_,          // mprescripts
    nsGkAtoms::mroot_,                // mroot
    nsGkAtoms::mrow_,                 // mrow
    nsGkAtoms::ms_,                   // ms
    nsGkAtoms::mscarries_,            // mscarries
    nsGkAtoms::mscarry_,              // mscarry
    nsGkAtoms::msgroup_,              // msgroup
    nsGkAtoms::msline_,               // msline
    nsGkAtoms::mspace_,               // mspace
    nsGkAtoms::msqrt_,                // msqrt
    nsGkAtoms::msrow_,                // msrow
    nsGkAtoms::mstack_,               // mstack
    nsGkAtoms::mstyle_,               // mstyle
    nsGkAtoms::msub_,                 // msub
    nsGkAtoms::msubsup_,              // msubsup
    nsGkAtoms::msup_,                 // msup
    nsGkAtoms::mtable_,               // mtable
    nsGkAtoms::mtd_,                  // mtd
    nsGkAtoms::mtext_,                // mtext
    nsGkAtoms::mtr_,                  // mtr
    nsGkAtoms::munder_,               // munder
    nsGkAtoms::munderover_,           // munderover
    nsGkAtoms::naturalnumbers_,       // naturalnumbers
    nsGkAtoms::neq_,                  // neq
    nsGkAtoms::none,                  // none
    nsGkAtoms::_not,                  // not
    nsGkAtoms::notanumber_,           // notanumber
    nsGkAtoms::note_,                 // note
    nsGkAtoms::notin_,                // notin
    nsGkAtoms::notprsubset_,          // notprsubset
    nsGkAtoms::notsubset_,            // notsubset
    nsGkAtoms::_or,                   // or
    nsGkAtoms::otherwise,             // otherwise
    nsGkAtoms::outerproduct_,         // outerproduct
    nsGkAtoms::partialdiff_,          // partialdiff
    nsGkAtoms::pi_,                   // pi
    nsGkAtoms::piece_,                // piece
    nsGkAtoms::piecewise_,            // piecewise
    nsGkAtoms::plus_,                 // plus
    nsGkAtoms::power_,                // power
    nsGkAtoms::primes_,               // primes
    nsGkAtoms::product_,              // product
    nsGkAtoms::prsubset_,             // prsubset
    nsGkAtoms::quotient_,             // quotient
    nsGkAtoms::rationals_,            // rationals
    nsGkAtoms::real_,                 // real
    nsGkAtoms::reals_,                // reals
    nsGkAtoms::reln_,                 // reln
    nsGkAtoms::rem,                   // rem
    nsGkAtoms::root_,                 // root
    nsGkAtoms::scalarproduct_,        // scalarproduct
    nsGkAtoms::sdev_,                 // sdev
    nsGkAtoms::sec_,                  // sec
    nsGkAtoms::sech_,                 // sech
    nsGkAtoms::selector_,             // selector
    nsGkAtoms::semantics_,            // semantics
    nsGkAtoms::sep_,                  // sep
    nsGkAtoms::set,                   // set
    nsGkAtoms::setdiff_,              // setdiff
    nsGkAtoms::share_,                // share
    nsGkAtoms::sin_,                  // sin
    nsGkAtoms::sinh_,                 // sinh
    nsGkAtoms::subset_,               // subset
    nsGkAtoms::sum,                   // sum
    nsGkAtoms::tan_,                  // tan
    nsGkAtoms::tanh_,                 // tanh
    nsGkAtoms::tendsto_,              // tendsto
    nsGkAtoms::times_,                // times
    nsGkAtoms::transpose_,            // transpose
    nsGkAtoms::_true,                 // true
    nsGkAtoms::union_,                // union
    nsGkAtoms::uplimit_,              // uplimit
    nsGkAtoms::variance_,             // variance
    nsGkAtoms::vector_,               // vector
    nsGkAtoms::vectorproduct_,        // vectorproduct
    nsGkAtoms::xor_,                  // xor
    nullptr};

const nsStaticAtom* const kAttributesMathML[] = {
    nsGkAtoms::accent_,                // accent
    nsGkAtoms::accentunder_,           // accentunder
    nsGkAtoms::actiontype_,            // actiontype
    nsGkAtoms::align,                  // align
    nsGkAtoms::alignmentscope_,        // alignmentscope
    nsGkAtoms::alt,                    // alt
    nsGkAtoms::altimg_,                // altimg
    nsGkAtoms::altimg_height_,         // altimg-height
    nsGkAtoms::altimg_valign_,         // altimg-valign
    nsGkAtoms::altimg_width_,          // altimg-width
    nsGkAtoms::background,             // background
    nsGkAtoms::base,                   // base
    nsGkAtoms::bevelled_,              // bevelled
    nsGkAtoms::cd_,                    // cd
    nsGkAtoms::cdgroup_,               // cdgroup
    nsGkAtoms::charalign_,             // charalign
    nsGkAtoms::close,                  // close
    nsGkAtoms::closure_,               // closure
    nsGkAtoms::color,                  // color
    nsGkAtoms::columnalign_,           // columnalign
    nsGkAtoms::columnalignment_,       // columnalignment
    nsGkAtoms::columnlines_,           // columnlines
    nsGkAtoms::columnspacing_,         // columnspacing
    nsGkAtoms::columnspan_,            // columnspan
    nsGkAtoms::columnwidth_,           // columnwidth
    nsGkAtoms::crossout_,              // crossout
    nsGkAtoms::decimalpoint_,          // decimalpoint
    nsGkAtoms::definitionURL_,         // definitionURL
    nsGkAtoms::denomalign_,            // denomalign
    nsGkAtoms::depth_,                 // depth
    nsGkAtoms::dir,                    // dir
    nsGkAtoms::display,                // display
    nsGkAtoms::displaystyle_,          // displaystyle
    nsGkAtoms::edge_,                  // edge
    nsGkAtoms::encoding,               // encoding
    nsGkAtoms::equalcolumns_,          // equalcolumns
    nsGkAtoms::equalrows_,             // equalrows
    nsGkAtoms::fence_,                 // fence
    nsGkAtoms::fontfamily_,            // fontfamily
    nsGkAtoms::fontsize_,              // fontsize
    nsGkAtoms::fontstyle_,             // fontstyle
    nsGkAtoms::fontweight_,            // fontweight
    nsGkAtoms::form,                   // form
    nsGkAtoms::frame,                  // frame
    nsGkAtoms::framespacing_,          // framespacing
    nsGkAtoms::groupalign_,            // groupalign
    nsGkAtoms::height,                 // height
    nsGkAtoms::href,                   // href
    nsGkAtoms::id,                     // id
    nsGkAtoms::indentalign_,           // indentalign
    nsGkAtoms::indentalignfirst_,      // indentalignfirst
    nsGkAtoms::indentalignlast_,       // indentalignlast
    nsGkAtoms::indentshift_,           // indentshift
    nsGkAtoms::indentshiftfirst_,      // indentshiftfirst
    nsGkAtoms::indenttarget_,          // indenttarget
    nsGkAtoms::index,                  // index
    nsGkAtoms::integer,                // integer
    nsGkAtoms::largeop_,               // largeop
    nsGkAtoms::length,                 // length
    nsGkAtoms::linebreak_,             // linebreak
    nsGkAtoms::linebreakmultchar_,     // linebreakmultchar
    nsGkAtoms::linebreakstyle_,        // linebreakstyle
    nsGkAtoms::linethickness_,         // linethickness
    nsGkAtoms::location_,              // location
    nsGkAtoms::longdivstyle_,          // longdivstyle
    nsGkAtoms::lquote_,                // lquote
    nsGkAtoms::lspace_,                // lspace
    nsGkAtoms::ltr,                    // ltr
    nsGkAtoms::mathbackground_,        // mathbackground
    nsGkAtoms::mathcolor_,             // mathcolor
    nsGkAtoms::mathsize_,              // mathsize
    nsGkAtoms::mathvariant_,           // mathvariant
    nsGkAtoms::maxsize_,               // maxsize
    nsGkAtoms::minlabelspacing_,       // minlabelspacing
    nsGkAtoms::minsize_,               // minsize
    nsGkAtoms::movablelimits_,         // movablelimits
    nsGkAtoms::msgroup_,               // msgroup
    nsGkAtoms::name,                   // name
    nsGkAtoms::newline,                // newline
    nsGkAtoms::notation_,              // notation
    nsGkAtoms::numalign_,              // numalign
    nsGkAtoms::number,                 // number
    nsGkAtoms::open,                   // open
    nsGkAtoms::order,                  // order
    nsGkAtoms::other,                  // other
    nsGkAtoms::overflow,               // overflow
    nsGkAtoms::position,               // position
    nsGkAtoms::role,                   // role
    nsGkAtoms::rowalign_,              // rowalign
    nsGkAtoms::rowlines_,              // rowlines
    nsGkAtoms::rowspacing_,            // rowspacing
    nsGkAtoms::rowspan,                // rowspan
    nsGkAtoms::rquote_,                // rquote
    nsGkAtoms::rspace_,                // rspace
    nsGkAtoms::schemaLocation_,        // schemaLocation
    nsGkAtoms::scriptlevel_,           // scriptlevel
    nsGkAtoms::scriptminsize_,         // scriptminsize
    nsGkAtoms::scriptsize_,            // scriptsize
    nsGkAtoms::scriptsizemultiplier_,  // scriptsizemultiplier
    nsGkAtoms::selection_,             // selection
    nsGkAtoms::separator_,             // separator
    nsGkAtoms::separators_,            // separators
    nsGkAtoms::shift_,                 // shift
    nsGkAtoms::side_,                  // side
    nsGkAtoms::src,                    // src
    nsGkAtoms::stackalign_,            // stackalign
    nsGkAtoms::stretchy_,              // stretchy
    nsGkAtoms::subscriptshift_,        // subscriptshift
    nsGkAtoms::superscriptshift_,      // superscriptshift
    nsGkAtoms::symmetric_,             // symmetric
    nsGkAtoms::type,                   // type
    nsGkAtoms::voffset_,               // voffset
    nsGkAtoms::width,                  // width
    nsGkAtoms::xref_,                  // xref
    nullptr};

const nsStaticAtom* const kURLAttributesMathML[] = {
    // clang-format off
  nsGkAtoms::href,
  nsGkAtoms::src,
  nsGkAtoms::cdgroup_,
  nsGkAtoms::altimg_,
  nsGkAtoms::definitionURL_,
  nullptr
    // clang-format on
};

// https://wicg.github.io/sanitizer-api/#baseline-attribute-allow-list
constexpr const nsStaticAtom* const kBaselineAttributeAllowlist[] = {
    // clang-format off
  nsGkAtoms::abbr,
  nsGkAtoms::accept,
  nsGkAtoms::acceptcharset,
  nsGkAtoms::charset,
  nsGkAtoms::accesskey,
  nsGkAtoms::action,
  nsGkAtoms::align,
  nsGkAtoms::alink,
  nsGkAtoms::allow,
  nsGkAtoms::allowfullscreen,
  // nsGkAtoms::allowpaymentrequest,
  nsGkAtoms::alt,
  nsGkAtoms::anchor,
  nsGkAtoms::archive,
  nsGkAtoms::as,
  nsGkAtoms::async,
  nsGkAtoms::autocapitalize,
  nsGkAtoms::autocomplete,
  // nsGkAtoms::autocorrect,
  nsGkAtoms::autofocus,
  // nsGkAtoms::autopictureinpicture,
  nsGkAtoms::autoplay,
  nsGkAtoms::axis,
  nsGkAtoms::background,
  nsGkAtoms::behavior,
  nsGkAtoms::bgcolor,
  nsGkAtoms::border,
  nsGkAtoms::bordercolor,
  nsGkAtoms::capture,
  nsGkAtoms::cellpadding,
  nsGkAtoms::cellspacing,
  // nsGkAtoms::challenge,
  nsGkAtoms::_char,
  nsGkAtoms::charoff,
  nsGkAtoms::charset,
  nsGkAtoms::checked,
  nsGkAtoms::cite,
  nsGkAtoms::_class,
  nsGkAtoms::classid,
  nsGkAtoms::clear,
  nsGkAtoms::code,
  nsGkAtoms::codebase,
  nsGkAtoms::codetype,
  nsGkAtoms::color,
  nsGkAtoms::cols,
  nsGkAtoms::colspan,
  nsGkAtoms::compact,
  nsGkAtoms::content,
  nsGkAtoms::contenteditable,
  nsGkAtoms::controls,
  // nsGkAtoms::controlslist,
  // nsGkAtoms::conversiondestination,
  nsGkAtoms::coords,
  nsGkAtoms::crossorigin,
  nsGkAtoms::csp,
  nsGkAtoms::data,
  nsGkAtoms::datetime,
  nsGkAtoms::declare,
  nsGkAtoms::decoding,
  nsGkAtoms::_default,
  nsGkAtoms::defer,
  nsGkAtoms::dir,
  nsGkAtoms::direction,
  // nsGkAtoms::dirname,
  nsGkAtoms::disabled,
  // nsGkAtoms::disablepictureinpicture,
  // nsGkAtoms::disableremoteplayback,
  // nsGkAtoms::disallowdocumentaccess,
  nsGkAtoms::download,
  nsGkAtoms::draggable,
  // nsGkAtoms::elementtiming,
  nsGkAtoms::enctype,
  nsGkAtoms::end,
  nsGkAtoms::enterkeyhint,
  nsGkAtoms::event,
  nsGkAtoms::exportparts,
  nsGkAtoms::face,
  nsGkAtoms::_for,
  nsGkAtoms::form,
  nsGkAtoms::formaction,
  nsGkAtoms::formenctype,
  nsGkAtoms::formmethod,
  nsGkAtoms::formnovalidate,
  nsGkAtoms::formtarget,
  nsGkAtoms::frame,
  nsGkAtoms::frameborder,
  nsGkAtoms::headers,
  nsGkAtoms::height,
  nsGkAtoms::hidden,
  nsGkAtoms::high,
  nsGkAtoms::href,
  nsGkAtoms::hreflang,
  // nsGkAtoms::hreftranslate,
  nsGkAtoms::hspace,
  nsGkAtoms::http,
  // nsGkAtoms::equiv,
  nsGkAtoms::id,
  nsGkAtoms::imagesizes,
  nsGkAtoms::imagesrcset,
  // nsGkAtoms::importance,
  // nsGkAtoms::impressiondata,
  // nsGkAtoms::impressionexpiry,
  // nsGkAtoms::incremental,
  nsGkAtoms::inert,
  nsGkAtoms::inputmode,
  nsGkAtoms::integrity,
  // nsGkAtoms::invisible,
  nsGkAtoms::is,
  nsGkAtoms::ismap,
  // nsGkAtoms::keytype,
  nsGkAtoms::kind,
  nsGkAtoms::label,
  nsGkAtoms::lang,
  nsGkAtoms::language,
  // nsGkAtoms::latencyhint,
  nsGkAtoms::leftmargin,
  nsGkAtoms::link,
  // nsGkAtoms::list,
  nsGkAtoms::loading,
  nsGkAtoms::longdesc,
  nsGkAtoms::loop,
  nsGkAtoms::low,
  nsGkAtoms::lowsrc,
  nsGkAtoms::manifest,
  nsGkAtoms::marginheight,
  nsGkAtoms::marginwidth,
  nsGkAtoms::max,
  nsGkAtoms::maxlength,
  // nsGkAtoms::mayscript,
  nsGkAtoms::media,
  nsGkAtoms::method,
  nsGkAtoms::min,
  nsGkAtoms::minlength,
  nsGkAtoms::multiple,
  nsGkAtoms::muted,
  nsGkAtoms::name,
  nsGkAtoms::nohref,
  nsGkAtoms::nomodule,
  nsGkAtoms::nonce,
  nsGkAtoms::noresize,
  nsGkAtoms::noshade,
  nsGkAtoms::novalidate,
  nsGkAtoms::nowrap,
  nsGkAtoms::object,
  nsGkAtoms::open,
  nsGkAtoms::optimum,
  nsGkAtoms::part,
  nsGkAtoms::pattern,
  nsGkAtoms::ping,
  nsGkAtoms::placeholder,
  // nsGkAtoms::playsinline,
  // nsGkAtoms::policy,
  nsGkAtoms::poster,
  nsGkAtoms::preload,
  // nsGkAtoms::pseudo,
  nsGkAtoms::readonly,
  nsGkAtoms::referrerpolicy,
  nsGkAtoms::rel,
  // nsGkAtoms::reportingorigin,
  nsGkAtoms::required,
  nsGkAtoms::resources,
  nsGkAtoms::rev,
  nsGkAtoms::reversed,
  nsGkAtoms::role,
  nsGkAtoms::rows,
  nsGkAtoms::rowspan,
  nsGkAtoms::rules,
  nsGkAtoms::sandbox,
  nsGkAtoms::scheme,
  nsGkAtoms::scope,
  // nsGkAtoms::scopes,
  nsGkAtoms::scrollamount,
  nsGkAtoms::scrolldelay,
  nsGkAtoms::scrolling,
  nsGkAtoms::select,
  nsGkAtoms::selected,
  // nsGkAtoms::shadowroot,
  // nsGkAtoms::shadowrootdelegatesfocus,
  nsGkAtoms::shape,
  nsGkAtoms::size,
  nsGkAtoms::sizes,
  nsGkAtoms::slot,
  nsGkAtoms::span,
  nsGkAtoms::spellcheck,
  nsGkAtoms::src,
  nsGkAtoms::srcdoc,
  nsGkAtoms::srclang,
  nsGkAtoms::srcset,
  nsGkAtoms::standby,
  nsGkAtoms::start,
  nsGkAtoms::step,
  nsGkAtoms::style,
  nsGkAtoms::summary,
  nsGkAtoms::tabindex,
  nsGkAtoms::target,
  nsGkAtoms::text,
  nsGkAtoms::title,
  nsGkAtoms::topmargin,
  nsGkAtoms::translate,
  nsGkAtoms::truespeed,
  // nsGkAtoms::trusttoken,
  nsGkAtoms::type,
  nsGkAtoms::usemap,
  nsGkAtoms::valign,
  nsGkAtoms::value,
  nsGkAtoms::valuetype,
  nsGkAtoms::version,
  // nsGkAtoms::virtualkeyboardpolicy,
  nsGkAtoms::vlink,
  nsGkAtoms::vspace,
  nsGkAtoms::webkitdirectory,
  nsGkAtoms::width,
  nsGkAtoms::wrap,
    // clang-format on
};

// https://wicg.github.io/sanitizer-api/#baseline-elements
constexpr const nsStaticAtom* const kBaselineElementAllowlist[] = {
    nsGkAtoms::a,          nsGkAtoms::abbr,      nsGkAtoms::acronym,
    nsGkAtoms::address,    nsGkAtoms::area,      nsGkAtoms::article,
    nsGkAtoms::aside,      nsGkAtoms::audio,     nsGkAtoms::b,
    nsGkAtoms::basefont,   nsGkAtoms::bdi,       nsGkAtoms::bdo,
    nsGkAtoms::bgsound,    nsGkAtoms::big,       nsGkAtoms::blockquote,
    nsGkAtoms::body,       nsGkAtoms::br,        nsGkAtoms::button,
    nsGkAtoms::canvas,     nsGkAtoms::caption,   nsGkAtoms::center,
    nsGkAtoms::cite,       nsGkAtoms::code,      nsGkAtoms::col,
    nsGkAtoms::colgroup,   nsGkAtoms::command,   nsGkAtoms::data,
    nsGkAtoms::datalist,   nsGkAtoms::dd,        nsGkAtoms::del,
    nsGkAtoms::details,    nsGkAtoms::dfn,       nsGkAtoms::dialog,
    nsGkAtoms::dir,        nsGkAtoms::div,       nsGkAtoms::dl,
    nsGkAtoms::dt,         nsGkAtoms::em,        nsGkAtoms::fieldset,
    nsGkAtoms::figcaption, nsGkAtoms::figure,    nsGkAtoms::font,
    nsGkAtoms::footer,     nsGkAtoms::form,      nsGkAtoms::h1,
    nsGkAtoms::h2,         nsGkAtoms::h3,        nsGkAtoms::h4,
    nsGkAtoms::h5,         nsGkAtoms::h6,        nsGkAtoms::head,
    nsGkAtoms::header,     nsGkAtoms::hgroup,    nsGkAtoms::hr,
    nsGkAtoms::html,       nsGkAtoms::i,         nsGkAtoms::image,
    nsGkAtoms::img,        nsGkAtoms::input,     nsGkAtoms::ins,
    nsGkAtoms::kbd,        nsGkAtoms::keygen,    nsGkAtoms::label,
    nsGkAtoms::layer,      nsGkAtoms::legend,    nsGkAtoms::li,
    nsGkAtoms::link,       nsGkAtoms::listing,   nsGkAtoms::main,
    nsGkAtoms::map,        nsGkAtoms::mark,      nsGkAtoms::marquee,
    nsGkAtoms::menu,       nsGkAtoms::meta,      nsGkAtoms::meter,
    nsGkAtoms::nav,        nsGkAtoms::nobr,      nsGkAtoms::ol,
    nsGkAtoms::optgroup,   nsGkAtoms::option,    nsGkAtoms::output,
    nsGkAtoms::p,          nsGkAtoms::picture,   nsGkAtoms::plaintext,
    nsGkAtoms::popup,      nsGkAtoms::portal,    nsGkAtoms::pre,
    nsGkAtoms::progress,   nsGkAtoms::q,         nsGkAtoms::rb,
    nsGkAtoms::rp,         nsGkAtoms::rt,        nsGkAtoms::rtc,
    nsGkAtoms::ruby,       nsGkAtoms::s,         nsGkAtoms::samp,
    nsGkAtoms::section,    nsGkAtoms::select,    nsGkAtoms::selectmenu,
    nsGkAtoms::slot,       nsGkAtoms::small,     nsGkAtoms::source,
    nsGkAtoms::span,       nsGkAtoms::strike,    nsGkAtoms::strong,
    nsGkAtoms::style,      nsGkAtoms::sub,       nsGkAtoms::summary,
    nsGkAtoms::sup,        nsGkAtoms::table,     nsGkAtoms::tbody,
    nsGkAtoms::td,         nsGkAtoms::_template, nsGkAtoms::textarea,
    nsGkAtoms::tfoot,      nsGkAtoms::th,        nsGkAtoms::thead,
    nsGkAtoms::time,       nsGkAtoms::title,     nsGkAtoms::tr,
    nsGkAtoms::track,      nsGkAtoms::tt,        nsGkAtoms::u,
    nsGkAtoms::ul,         nsGkAtoms::var,       nsGkAtoms::video,
    nsGkAtoms::wbr,        nsGkAtoms::xmp,
};

// https://wicg.github.io/sanitizer-api/#default-configuration
// default configuration's attribute allow list.
// Note: Currently all listed attributes are allowed for every element
// (e.g. they use "*").
// Compared to kBaselineAttributeAllowlist only deprecated allowpaymentrequest
// attribute is missing.
constexpr const nsStaticAtom* const kDefaultConfigurationAttributeAllowlist[] =
    {
        nsGkAtoms::abbr,
        nsGkAtoms::accept,
        nsGkAtoms::acceptcharset,
        nsGkAtoms::charset,
        nsGkAtoms::accesskey,
        nsGkAtoms::action,
        nsGkAtoms::align,
        nsGkAtoms::alink,
        nsGkAtoms::allow,
        nsGkAtoms::allowfullscreen,
        nsGkAtoms::alt,
        nsGkAtoms::anchor,
        nsGkAtoms::archive,
        nsGkAtoms::as,
        nsGkAtoms::async,
        nsGkAtoms::autocapitalize,
        nsGkAtoms::autocomplete,
        // nsGkAtoms::autocorrect,
        nsGkAtoms::autofocus,
        // nsGkAtoms::autopictureinpicture,
        nsGkAtoms::autoplay,
        nsGkAtoms::axis,
        nsGkAtoms::background,
        nsGkAtoms::behavior,
        nsGkAtoms::bgcolor,
        nsGkAtoms::border,
        nsGkAtoms::bordercolor,
        nsGkAtoms::capture,
        nsGkAtoms::cellpadding,
        nsGkAtoms::cellspacing,
        // nsGkAtoms::challenge,
        nsGkAtoms::_char,
        nsGkAtoms::charoff,
        nsGkAtoms::charset,
        nsGkAtoms::checked,
        nsGkAtoms::cite,
        nsGkAtoms::_class,
        nsGkAtoms::classid,
        nsGkAtoms::clear,
        nsGkAtoms::code,
        nsGkAtoms::codebase,
        nsGkAtoms::codetype,
        nsGkAtoms::color,
        nsGkAtoms::cols,
        nsGkAtoms::colspan,
        nsGkAtoms::compact,
        nsGkAtoms::content,
        nsGkAtoms::contenteditable,
        nsGkAtoms::controls,
        // nsGkAtoms::controlslist,
        // nsGkAtoms::conversiondestination,
        nsGkAtoms::coords,
        nsGkAtoms::crossorigin,
        nsGkAtoms::csp,
        nsGkAtoms::data,
        nsGkAtoms::datetime,
        nsGkAtoms::declare,
        nsGkAtoms::decoding,
        nsGkAtoms::_default,
        nsGkAtoms::defer,
        nsGkAtoms::dir,
        nsGkAtoms::direction,
        // nsGkAtoms::dirname,
        nsGkAtoms::disabled,
        // nsGkAtoms::disablepictureinpicture,
        // nsGkAtoms::disableremoteplayback,
        // nsGkAtoms::disallowdocumentaccess,
        nsGkAtoms::download,
        nsGkAtoms::draggable,
        // nsGkAtoms::elementtiming,
        nsGkAtoms::enctype,
        nsGkAtoms::end,
        nsGkAtoms::enterkeyhint,
        nsGkAtoms::event,
        nsGkAtoms::exportparts,
        nsGkAtoms::face,
        nsGkAtoms::_for,
        nsGkAtoms::form,
        nsGkAtoms::formaction,
        nsGkAtoms::formenctype,
        nsGkAtoms::formmethod,
        nsGkAtoms::formnovalidate,
        nsGkAtoms::formtarget,
        nsGkAtoms::frame,
        nsGkAtoms::frameborder,
        nsGkAtoms::headers,
        nsGkAtoms::height,
        nsGkAtoms::hidden,
        nsGkAtoms::high,
        nsGkAtoms::href,
        nsGkAtoms::hreflang,
        // nsGkAtoms::hreftranslate,
        nsGkAtoms::hspace,
        nsGkAtoms::http,
        // nsGkAtoms::equiv,
        nsGkAtoms::id,
        nsGkAtoms::imagesizes,
        nsGkAtoms::imagesrcset,
        // nsGkAtoms::importance,
        // nsGkAtoms::impressiondata,
        // nsGkAtoms::impressionexpiry,
        // nsGkAtoms::incremental,
        nsGkAtoms::inert,
        nsGkAtoms::inputmode,
        nsGkAtoms::integrity,
        // nsGkAtoms::invisible,
        nsGkAtoms::is,
        nsGkAtoms::ismap,
        // nsGkAtoms::keytype,
        nsGkAtoms::kind,
        nsGkAtoms::label,
        nsGkAtoms::lang,
        nsGkAtoms::language,
        // nsGkAtoms::latencyhint,
        nsGkAtoms::leftmargin,
        nsGkAtoms::link,
        // nsGkAtoms::list,
        nsGkAtoms::loading,
        nsGkAtoms::longdesc,
        nsGkAtoms::loop,
        nsGkAtoms::low,
        nsGkAtoms::lowsrc,
        nsGkAtoms::manifest,
        nsGkAtoms::marginheight,
        nsGkAtoms::marginwidth,
        nsGkAtoms::max,
        nsGkAtoms::maxlength,
        // nsGkAtoms::mayscript,
        nsGkAtoms::media,
        nsGkAtoms::method,
        nsGkAtoms::min,
        nsGkAtoms::minlength,
        nsGkAtoms::multiple,
        nsGkAtoms::muted,
        nsGkAtoms::name,
        nsGkAtoms::nohref,
        nsGkAtoms::nomodule,
        nsGkAtoms::nonce,
        nsGkAtoms::noresize,
        nsGkAtoms::noshade,
        nsGkAtoms::novalidate,
        nsGkAtoms::nowrap,
        nsGkAtoms::object,
        nsGkAtoms::open,
        nsGkAtoms::optimum,
        nsGkAtoms::part,
        nsGkAtoms::pattern,
        nsGkAtoms::ping,
        nsGkAtoms::placeholder,
        // nsGkAtoms::playsinline,
        // nsGkAtoms::policy,
        nsGkAtoms::poster,
        nsGkAtoms::preload,
        // nsGkAtoms::pseudo,
        nsGkAtoms::readonly,
        nsGkAtoms::referrerpolicy,
        nsGkAtoms::rel,
        // nsGkAtoms::reportingorigin,
        nsGkAtoms::required,
        nsGkAtoms::resources,
        nsGkAtoms::rev,
        nsGkAtoms::reversed,
        nsGkAtoms::role,
        nsGkAtoms::rows,
        nsGkAtoms::rowspan,
        nsGkAtoms::rules,
        nsGkAtoms::sandbox,
        nsGkAtoms::scheme,
        nsGkAtoms::scope,
        // nsGkAtoms::scopes,
        nsGkAtoms::scrollamount,
        nsGkAtoms::scrolldelay,
        nsGkAtoms::scrolling,
        nsGkAtoms::select,
        nsGkAtoms::selected,
        // nsGkAtoms::shadowroot,
        // nsGkAtoms::shadowrootdelegatesfocus,
        nsGkAtoms::shape,
        nsGkAtoms::size,
        nsGkAtoms::sizes,
        nsGkAtoms::slot,
        nsGkAtoms::span,
        nsGkAtoms::spellcheck,
        nsGkAtoms::src,
        nsGkAtoms::srcdoc,
        nsGkAtoms::srclang,
        nsGkAtoms::srcset,
        nsGkAtoms::standby,
        nsGkAtoms::start,
        nsGkAtoms::step,
        nsGkAtoms::style,
        nsGkAtoms::summary,
        nsGkAtoms::tabindex,
        nsGkAtoms::target,
        nsGkAtoms::text,
        nsGkAtoms::title,
        nsGkAtoms::topmargin,
        nsGkAtoms::translate,
        nsGkAtoms::truespeed,
        // nsGkAtoms::trusttoken,
        nsGkAtoms::type,
        nsGkAtoms::usemap,
        nsGkAtoms::valign,
        nsGkAtoms::value,
        nsGkAtoms::valuetype,
        nsGkAtoms::version,
        // nsGkAtoms::virtualkeyboardpolicy,
        nsGkAtoms::vlink,
        nsGkAtoms::vspace,
        nsGkAtoms::webkitdirectory,
        nsGkAtoms::width,
        nsGkAtoms::wrap,
};

// https://wicg.github.io/sanitizer-api/#default-configuration
// default configuration's element allow list.
constexpr const nsStaticAtom* const kDefaultConfigurationElementAllowlist[] = {
    nsGkAtoms::a,          nsGkAtoms::abbr,       nsGkAtoms::acronym,
    nsGkAtoms::address,    nsGkAtoms::area,       nsGkAtoms::article,
    nsGkAtoms::aside,      nsGkAtoms::audio,      nsGkAtoms::b,
    nsGkAtoms::bdi,        nsGkAtoms::bdo,        nsGkAtoms::bgsound,
    nsGkAtoms::big,        nsGkAtoms::blockquote, nsGkAtoms::body,
    nsGkAtoms::br,         nsGkAtoms::button,     nsGkAtoms::canvas,
    nsGkAtoms::caption,    nsGkAtoms::center,     nsGkAtoms::cite,
    nsGkAtoms::code,       nsGkAtoms::col,        nsGkAtoms::colgroup,
    nsGkAtoms::datalist,   nsGkAtoms::dd,         nsGkAtoms::del,
    nsGkAtoms::details,    nsGkAtoms::dfn,        nsGkAtoms::dialog,
    nsGkAtoms::dir,        nsGkAtoms::div,        nsGkAtoms::dl,
    nsGkAtoms::dt,         nsGkAtoms::em,         nsGkAtoms::fieldset,
    nsGkAtoms::figcaption, nsGkAtoms::figure,     nsGkAtoms::font,
    nsGkAtoms::footer,     nsGkAtoms::form,       nsGkAtoms::h1,
    nsGkAtoms::h2,         nsGkAtoms::h3,         nsGkAtoms::h4,
    nsGkAtoms::h5,         nsGkAtoms::h6,         nsGkAtoms::head,
    nsGkAtoms::header,     nsGkAtoms::hgroup,     nsGkAtoms::hr,
    nsGkAtoms::html,       nsGkAtoms::i,          nsGkAtoms::img,
    nsGkAtoms::input,      nsGkAtoms::ins,        nsGkAtoms::kbd,
    nsGkAtoms::keygen,     nsGkAtoms::label,      nsGkAtoms::layer,
    nsGkAtoms::legend,     nsGkAtoms::li,         nsGkAtoms::link,
    nsGkAtoms::listing,    nsGkAtoms::main,       nsGkAtoms::map,
    nsGkAtoms::mark,       nsGkAtoms::marquee,    nsGkAtoms::menu,
    nsGkAtoms::meta,       nsGkAtoms::meter,      nsGkAtoms::nav,
    nsGkAtoms::nobr,       nsGkAtoms::ol,         nsGkAtoms::optgroup,
    nsGkAtoms::option,     nsGkAtoms::output,     nsGkAtoms::p,
    nsGkAtoms::picture,    nsGkAtoms::popup,      nsGkAtoms::pre,
    nsGkAtoms::progress,   nsGkAtoms::q,          nsGkAtoms::rb,
    nsGkAtoms::rp,         nsGkAtoms::rt,         nsGkAtoms::rtc,
    nsGkAtoms::ruby,       nsGkAtoms::s,          nsGkAtoms::samp,
    nsGkAtoms::section,    nsGkAtoms::select,     nsGkAtoms::selectmenu,
    nsGkAtoms::small,      nsGkAtoms::source,     nsGkAtoms::span,
    nsGkAtoms::strike,     nsGkAtoms::strong,     nsGkAtoms::style,
    nsGkAtoms::sub,        nsGkAtoms::summary,    nsGkAtoms::sup,
    nsGkAtoms::table,      nsGkAtoms::tbody,      nsGkAtoms::td,
    nsGkAtoms::tfoot,      nsGkAtoms::th,         nsGkAtoms::thead,
    nsGkAtoms::time,       nsGkAtoms::tr,         nsGkAtoms::track,
    nsGkAtoms::tt,         nsGkAtoms::u,          nsGkAtoms::ul,
    nsGkAtoms::var,        nsGkAtoms::video,      nsGkAtoms::wbr,
};

nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sElementsHTML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sAttributesHTML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sPresAttributesHTML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sElementsSVG = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sAttributesSVG = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sElementsMathML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sAttributesMathML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sBaselineAttributeAllowlist =
    nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sBaselineElementAllowlist =
    nullptr;
nsTreeSanitizer::AtomsTable*
    nsTreeSanitizer::sDefaultConfigurationAttributeAllowlist = nullptr;
nsTreeSanitizer::AtomsTable*
    nsTreeSanitizer::sDefaultConfigurationElementAllowlist = nullptr;
nsIPrincipal* nsTreeSanitizer::sNullPrincipal = nullptr;

nsTreeSanitizer::nsTreeSanitizer(uint32_t aFlags)
    : mAllowStyles(aFlags & nsIParserUtils::SanitizerAllowStyle),
      mAllowComments(aFlags & nsIParserUtils::SanitizerAllowComments),
      mDropNonCSSPresentation(aFlags &
                              nsIParserUtils::SanitizerDropNonCSSPresentation),
      mDropForms(aFlags & nsIParserUtils::SanitizerDropForms),
      mCidEmbedsOnly(aFlags & nsIParserUtils::SanitizerCidEmbedsOnly),
      mDropMedia(aFlags & nsIParserUtils::SanitizerDropMedia),
      mFullDocument(false),
      mLogRemovals(aFlags & nsIParserUtils::SanitizerLogRemovals) {
  if (mCidEmbedsOnly) {
    // Sanitizing styles for external references is not supported.
    mAllowStyles = false;
  }

  if (!sElementsHTML) {
    // Initialize lazily to avoid having to initialize at all if the user
    // doesn't paste HTML or load feeds.
    InitializeStatics();
  }
}

bool nsTreeSanitizer::MustFlatten(int32_t aNamespace, nsAtom* aLocal) {
  if (mIsForSanitizerAPI) {
    return MustFlattenForSanitizerAPI(aNamespace, aLocal);
  }

  if (aNamespace == kNameSpaceID_XHTML) {
    if (mDropNonCSSPresentation &&
        (nsGkAtoms::font == aLocal || nsGkAtoms::center == aLocal)) {
      return true;
    }
    if (mDropForms &&
        (nsGkAtoms::form == aLocal || nsGkAtoms::input == aLocal ||
         nsGkAtoms::option == aLocal || nsGkAtoms::optgroup == aLocal)) {
      return true;
    }
    if (mFullDocument &&
        (nsGkAtoms::title == aLocal || nsGkAtoms::html == aLocal ||
         nsGkAtoms::head == aLocal || nsGkAtoms::body == aLocal)) {
      return false;
    }
    if (nsGkAtoms::_template == aLocal) {
      return false;
    }
    return !sElementsHTML->Contains(aLocal);
  }
  if (aNamespace == kNameSpaceID_SVG) {
    if (mCidEmbedsOnly || mDropMedia) {
      // Sanitizing CSS-based URL references inside SVG presentational
      // attributes is not supported, so flattening for cid: embed case.
      return true;
    }
    return !sElementsSVG->Contains(aLocal);
  }
  if (aNamespace == kNameSpaceID_MathML) {
    return !sElementsMathML->Contains(aLocal);
  }
  return true;
}

bool nsTreeSanitizer::MustFlattenForSanitizerAPI(int32_t aNamespace,
                                                 nsAtom* aLocal) {
  // This implements everything in
  // https://wicg.github.io/sanitizer-api/#sanitize-action-for-an-element that
  // is supposed to be blocked.

  // Step 6. If element matches any name in config["blockElements"]: Return
  // block.
  if (mBlockElements &&
      MatchesElementName(*mBlockElements, aNamespace, aLocal)) {
    return true;
  }

  // Step 7. Let allow list be null.
  // Step 8. If "allowElements" exists in config:
  // Step 8.1. Then : Set allow list to config["allowElements"].
  if (mAllowElements) {
    // Step 9. If element does not match any name in allow list:
    // Return block.
    if (!MatchesElementName(*mAllowElements, aNamespace, aLocal)) {
      return true;
    }
  } else {
    // Step 8.2. Otherwise: Set allow list to the default configuration's
    // element allow list.

    // Step 9. If element does not match any name in allow list:
    // Return block.

    // The default configuration only contains HTML elements, so we can
    // reject everything else.
    if (aNamespace != kNameSpaceID_XHTML ||
        !sDefaultConfigurationElementAllowlist->Contains(aLocal)) {
      return true;
    }
  }

  // Step 10. Return keep.
  return false;
}

bool nsTreeSanitizer::IsURL(const nsStaticAtom* const* aURLs,
                            nsAtom* aLocalName) {
  const nsStaticAtom* atom;
  while ((atom = *aURLs)) {
    if (atom == aLocalName) {
      return true;
    }
    ++aURLs;
  }
  return false;
}

bool nsTreeSanitizer::MustPrune(int32_t aNamespace, nsAtom* aLocal,
                                mozilla::dom::Element* aElement) {
  if (mIsForSanitizerAPI) {
    return MustPruneForSanitizerAPI(aNamespace, aLocal, aElement);
  }

  // To avoid attacks where a MathML script becomes something that gets
  // serialized in a way that it parses back as an HTML script, let's just
  // drop elements with the local name 'script' regardless of namespace.
  if (nsGkAtoms::script == aLocal) {
    return true;
  }
  if (aNamespace == kNameSpaceID_XHTML) {
    if (nsGkAtoms::title == aLocal && !mFullDocument) {
      // emulate the quirks of the old parser
      return true;
    }
    if (mDropForms &&
        (nsGkAtoms::select == aLocal || nsGkAtoms::button == aLocal ||
         nsGkAtoms::datalist == aLocal)) {
      return true;
    }
    if (mDropMedia &&
        (nsGkAtoms::img == aLocal || nsGkAtoms::video == aLocal ||
         nsGkAtoms::audio == aLocal || nsGkAtoms::source == aLocal)) {
      return true;
    }
    if (nsGkAtoms::meta == aLocal &&
        (aElement->HasAttr(nsGkAtoms::charset) ||
         aElement->HasAttr(nsGkAtoms::httpEquiv))) {
      // Throw away charset declarations even if they also have microdata
      // which they can't validly have.
      return true;
    }
    if (((!mFullDocument && nsGkAtoms::meta == aLocal) ||
         nsGkAtoms::link == aLocal) &&
        !(aElement->HasAttr(nsGkAtoms::itemprop) ||
          aElement->HasAttr(nsGkAtoms::itemscope))) {
      // emulate old behavior for non-Microdata <meta> and <link> presumably
      // in <head>. <meta> and <link> are whitelisted in order to avoid
      // corrupting Microdata when they appear in <body>. Note that
      // SanitizeAttributes() will remove the rel attribute from <link> and
      // the name attribute from <meta>.
      return true;
    }
  }
  if (mAllowStyles) {
    return nsGkAtoms::style == aLocal && !(aNamespace == kNameSpaceID_XHTML ||
                                           aNamespace == kNameSpaceID_SVG);
  }
  if (nsGkAtoms::style == aLocal) {
    return true;
  }
  return false;
}

enum class ElementKind {
  Regular,
  Custom,
  Unknown,
};

// https://wicg.github.io/sanitizer-api/#element-kind
static ElementKind GetElementKind(int32_t aNamespace, nsAtom* aLocal,
                                  Element* aElement) {
  // XXX(bug 1782926) The spec for this is known to be wrong.
  // https://github.com/WICG/sanitizer-api/issues/147

  // custom, if element’s local name is a valid custom element name,
  // XXX shouldn't this happen after unknown.
  if (nsContentUtils::IsCustomElementName(aLocal, kNameSpaceID_XHTML)) {
    return ElementKind::Custom;
  }

  // unknown, if element is not in the [HTML] namespace
  // XXX this doesn't really make sense to me
  // https://github.com/WICG/sanitizer-api/issues/167
  if (aNamespace != kNameSpaceID_XHTML) {
    return ElementKind::Unknown;
  }

  // or if element’s local name denotes an unknown element
  // — that is, if the element interface the [HTML] specification assigns to it
  // would be HTMLUnknownElement,
  if (nsCOMPtr<HTMLUnknownElement> el = do_QueryInterface(aElement)) {
    return ElementKind::Unknown;
  }

  // regular, otherwise.
  return ElementKind::Regular;
}

bool nsTreeSanitizer::MustPruneForSanitizerAPI(int32_t aNamespace,
                                               nsAtom* aLocal,
                                               Element* aElement) {
  // This implements everything in
  // https://wicg.github.io/sanitizer-api/#sanitize-action-for-an-element that
  // is supposed to be dropped.

  // Step 1. Let kind be element’s element kind.
  ElementKind kind = GetElementKind(aNamespace, aLocal, aElement);

  switch (kind) {
    case ElementKind::Regular:
      // Step 2. If kind is regular and element does not match any name in the
      // baseline element allow list: Return drop.
      if (!sBaselineElementAllowlist->Contains(aLocal)) {
        return true;
      }
      break;

    case ElementKind::Custom:
      // Step 3. If kind is custom and if config["allowCustomElements"] does not
      // exist or if config["allowCustomElements"] is false: Return drop.
      if (!mAllowCustomElements) {
        return true;
      }
      break;

    case ElementKind::Unknown:
      // Step 4. If kind is unknown and if config["allowUnknownMarkup"] does not
      // exist or it config["allowUnknownMarkup"] is false: Return drop.
      if (!mAllowUnknownMarkup) {
        return true;
      }
      break;
  }

  // Step 5. If element matches any name in config["dropElements"]: Return drop.
  if (mDropElements && MatchesElementName(*mDropElements, aNamespace, aLocal)) {
    return true;
  }

  return false;
}

/**
 * Parses a style sheet and reserializes it with unsafe styles removed.
 *
 * @param aOriginal the original style sheet source
 * @param aSanitized the reserialization without dangerous CSS.
 * @param aDocument the document the style sheet belongs to
 * @param aBaseURI the base URI to use
 * @param aSanitizationKind the kind of style sanitization to use.
 */
static void SanitizeStyleSheet(const nsAString& aOriginal,
                               nsAString& aSanitized, Document* aDocument,
                               nsIURI* aBaseURI,
                               StyleSanitizationKind aSanitizationKind) {
  aSanitized.Truncate();

  NS_ConvertUTF16toUTF8 style(aOriginal);
  nsIReferrerInfo* referrer =
      aDocument->ReferrerInfoForInternalCSSAndSVGResources();
  auto extraData =
      MakeRefPtr<URLExtraData>(aBaseURI, referrer, aDocument->NodePrincipal());
  RefPtr<StyleStylesheetContents> contents =
      Servo_StyleSheet_FromUTF8Bytes(
          /* loader = */ nullptr,
          /* stylesheet = */ nullptr,
          /* load_data = */ nullptr, &style,
          css::SheetParsingMode::eAuthorSheetFeatures, extraData.get(),
          aDocument->GetCompatibilityMode(),
          /* reusable_sheets = */ nullptr,
          /* use_counters = */ nullptr, StyleAllowImportRules::Yes,
          aSanitizationKind, &aSanitized)
          .Consume();
}

bool nsTreeSanitizer::SanitizeInlineStyle(
    Element* aElement, StyleSanitizationKind aSanitizationKind) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->IsHTMLElement(nsGkAtoms::style) ||
             aElement->IsSVGElement(nsGkAtoms::style));

  nsAutoString styleText;
  nsContentUtils::GetNodeTextContent(aElement, false, styleText);

  nsAutoString sanitizedStyle;
  SanitizeStyleSheet(styleText, sanitizedStyle, aElement->OwnerDoc(),
                     aElement->GetBaseURI(), StyleSanitizationKind::Standard);
  RemoveAllAttributesFromDescendants(aElement);
  nsContentUtils::SetNodeTextContent(aElement, sanitizedStyle, true);

  return sanitizedStyle.Length() != styleText.Length();
}

void nsTreeSanitizer::RemoveConditionalCSSFromSubtree(nsINode* aRoot) {
  AutoTArray<RefPtr<nsINode>, 10> nodesToSanitize;
  for (nsINode* node : ShadowIncludingTreeIterator(*aRoot)) {
    if (node->IsHTMLElement(nsGkAtoms::style) ||
        node->IsSVGElement(nsGkAtoms::style)) {
      nodesToSanitize.AppendElement(node);
    }
  }
  for (nsINode* node : nodesToSanitize) {
    SanitizeInlineStyle(node->AsElement(),
                        StyleSanitizationKind::NoConditionalRules);
  }
}

template <size_t Len>
static bool UTF16StringStartsWith(const char16_t* aStr, uint32_t aLength,
                                  const char16_t (&aNeedle)[Len]) {
  MOZ_ASSERT(aNeedle[Len - 1] == '\0',
             "needle should be a UTF-16 encoded string literal");

  if (aLength < Len - 1) {
    return false;
  }
  for (size_t i = 0; i < Len - 1; i++) {
    if (aStr[i] != aNeedle[i]) {
      return false;
    }
  }
  return true;
}

void nsTreeSanitizer::SanitizeAttributes(mozilla::dom::Element* aElement,
                                         AllowedAttributes aAllowed) {
  int32_t ac = (int)aElement->GetAttrCount();

  for (int32_t i = ac - 1; i >= 0; --i) {
    const nsAttrName* attrName = aElement->GetAttrNameAt(i);
    int32_t attrNs = attrName->NamespaceID();
    RefPtr<nsAtom> attrLocal = attrName->LocalName();

    if (mIsForSanitizerAPI) {
      if (MustDropAttribute(aElement, attrNs, attrLocal) ||
          MustDropFunkyAttribute(aElement, attrNs, attrLocal)) {
        aElement->UnsetAttr(kNameSpaceID_None, attrLocal, false);
        if (mLogRemovals) {
          LogMessage("Removed unsafe attribute.", aElement->OwnerDoc(),
                     aElement, attrLocal);
        }

        // in case the attribute removal shuffled the attribute order, start
        // the loop again.
        --ac;
        i = ac;  // i will be decremented immediately thanks to the for loop
      }
      continue;
    }

    if (kNameSpaceID_None == attrNs) {
      if (aAllowed.mStyle && nsGkAtoms::style == attrLocal) {
        continue;
      }
      if (aAllowed.mDangerousSrc && nsGkAtoms::src == attrLocal) {
        continue;
      }
      if (IsURL(aAllowed.mURLs, attrLocal)) {
        bool fragmentOnly = aElement->IsSVGElement(nsGkAtoms::use);
        if (SanitizeURL(aElement, attrNs, attrLocal, fragmentOnly)) {
          // in case the attribute removal shuffled the attribute order, start
          // the loop again.
          --ac;
          i = ac;  // i will be decremented immediately thanks to the for loop
          continue;
        }
        // else fall through to see if there's another reason to drop this
        // attribute (in particular if the attribute is background="" on an
        // HTML element)
      }
      if (!mDropNonCSSPresentation &&
          (aAllowed.mNames == sAttributesHTML) &&  // element is HTML
          sPresAttributesHTML->Contains(attrLocal)) {
        continue;
      }
      if (aAllowed.mNames->Contains(attrLocal) &&
          !((attrLocal == nsGkAtoms::rel &&
             aElement->IsHTMLElement(nsGkAtoms::link)) ||
            (!mFullDocument && attrLocal == nsGkAtoms::name &&
             aElement->IsHTMLElement(nsGkAtoms::meta)))) {
        // name="" and rel="" are whitelisted, but treat them as blacklisted
        // for <meta name> (fragment case) and <link rel> (all cases) to avoid
        // document-wide metadata or styling overrides with non-conforming
        // <meta name itemprop> or
        // <link rel itemprop>
        continue;
      }
      const char16_t* localStr = attrLocal->GetUTF16String();
      uint32_t localLen = attrLocal->GetLength();
      // Allow underscore to cater to the MCE editor library.
      // Allow data-* on SVG and MathML, too, as a forward-compat measure.
      // Allow aria-* on all for simplicity.
      if (UTF16StringStartsWith(localStr, localLen, u"_") ||
          UTF16StringStartsWith(localStr, localLen, u"data-") ||
          UTF16StringStartsWith(localStr, localLen, u"aria-")) {
        continue;
      }
      // else not allowed
    } else if (kNameSpaceID_XML == attrNs) {
      if (nsGkAtoms::lang == attrLocal || nsGkAtoms::space == attrLocal) {
        continue;
      }
      // else not allowed
    } else if (aAllowed.mXLink && kNameSpaceID_XLink == attrNs) {
      if (nsGkAtoms::href == attrLocal) {
        bool fragmentOnly = aElement->IsSVGElement(nsGkAtoms::use);
        if (SanitizeURL(aElement, attrNs, attrLocal, fragmentOnly)) {
          // in case the attribute removal shuffled the attribute order, start
          // the loop again.
          --ac;
          i = ac;  // i will be decremented immediately thanks to the for loop
        }
        continue;
      }
      if (nsGkAtoms::type == attrLocal || nsGkAtoms::title == attrLocal ||
          nsGkAtoms::show == attrLocal || nsGkAtoms::actuate == attrLocal) {
        continue;
      }
      // else not allowed
    }
    aElement->UnsetAttr(kNameSpaceID_None, attrLocal, false);
    if (mLogRemovals) {
      LogMessage("Removed unsafe attribute.", aElement->OwnerDoc(), aElement,
                 attrLocal);
    }
    // in case the attribute removal shuffled the attribute order, start the
    // loop again.
    --ac;
    i = ac;  // i will be decremented immediately thanks to the for loop
  }

  // If we've got HTML audio or video, add the controls attribute, because
  // otherwise the content is unplayable with scripts removed.
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::video, nsGkAtoms::audio)) {
    aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::controls, u""_ns, false);
  }
}

// https://wicg.github.io/sanitizer-api/#element-matches-an-element-name
bool nsTreeSanitizer::MatchesElementName(ElementNameSet& aNames,
                                         int32_t aNamespace,
                                         nsAtom* aLocalName) {
  return aNames.Contains(ElementName(aNamespace, aLocalName));
}

// https://wicg.github.io/sanitizer-api/#attribute-match-list
bool nsTreeSanitizer::MatchesAttributeMatchList(
    AttributesToElementsMap& aMatchList, Element& aElement,
    int32_t aAttrNamespace, nsAtom* aAttrLocalName) {
  // Step 1. If attribute’s local name does not match the attribute match list
  // list’s key and if the key is not "*": Return false.
  ElementNameSet* names;
  if (auto lookup =
          aMatchList.Lookup(AttributeName(aAttrNamespace, aAttrLocalName))) {
    names = lookup->get();
  } else {
    return false;
  }

  // Step 2. Let element be the attribute’s Element.
  // Step 3. Let element name be element’s local name.
  // Step 4. If element is a in either the SVG or MathML namespaces (i.e., it’s
  // a foreign element), then prefix element name with the appropriate namespace
  // designator plus a whitespace character.

  // TODO: This is spec text is going to change.
  int32_t namespaceID = aElement.NodeInfo()->NamespaceID();
  RefPtr<nsAtom> nameAtom = aElement.NodeInfo()->NameAtom();

  // Step 5. If list’s value does not contain element name and value is not
  // ["*"]: Return false.
  // Step 6. Return true.

  // nullptr means star (*), i.e. any element.
  if (!names) {
    return true;
  }
  return MatchesElementName(*names, namespaceID, nameAtom);
}

// https://wicg.github.io/sanitizer-api/#sanitize-action-for-an-attribute
bool nsTreeSanitizer::MustDropAttribute(Element* aElement,
                                        int32_t aAttrNamespace,
                                        nsAtom* aAttrLocalName) {
  // Step 1. Let kind be attribute’s attribute kind.
  // Step 2. If kind is unknown and if config["allowUnknownMarkup"] does not
  // exist or it config["allowUnknownMarkup"] is false: Return drop.
  //
  // TODO: Not clear how to determine if something is an "unknown" attribute.
  // https://github.com/WICG/sanitizer-api/issues/147 should probably define
  // an explicit list.

  // Step 3. If kind is regular and attribute’s local name does not match any
  // name in the baseline attribute allow list: Return drop.
  if (!sBaselineAttributeAllowlist->Contains(aAttrLocalName)) {
    return true;
  }

  // Step 4. If attribute matches any attribute match list in config’s attribute
  // drop list: Return drop.
  if (mDropAttributes &&
      MatchesAttributeMatchList(*mDropAttributes, *aElement, aAttrNamespace,
                                aAttrLocalName)) {
    return true;
  }

  // Step 5. If attribute allow list exists in config:
  if (mAllowAttributes) {
    // Step 5.1. Then let allow list be |config|["allowAttributes"].
    // Step 6. If attribute does not match any attribute match list in allow
    // list: Return drop.
    if (!MatchesAttributeMatchList(*mAllowAttributes, *aElement, aAttrNamespace,
                                   aAttrLocalName)) {
      return true;
    }
  } else {
    // Step 5.2. Otherwise: Let allow list be the default configuration's
    // attribute allow list.
    // Step 6. If attribute does not match any attribute
    // match list in allow list: Return drop.
    if (!sDefaultConfigurationAttributeAllowlist->Contains(aAttrLocalName)) {
      return true;
    }
  }

  // Step 7. Return keep.
  return false;
}

// https://wicg.github.io/sanitizer-api/#handle-funky-elements
bool nsTreeSanitizer::MustDropFunkyAttribute(Element* aElement,
                                             int32_t aAttrNamespace,
                                             nsAtom* aAttrLocalName) {
  // Step 1. If element’s element interface is HTMLTemplateElement:
  // Note: This step is implemented in the main loop of SanitizeChildren.

  // Step 2. If element’s element interface has a HTMLHyperlinkElementUtils
  // mixin, and if element’s protocol property is "javascript:":
  // TODO(https://github.com/WICG/sanitizer-api/issues/168)
  if (aAttrLocalName == nsGkAtoms::href) {
    if (nsCOMPtr<Link> link = do_QueryInterface(aElement)) {
      nsCOMPtr<nsIURI> uri = link->GetURI();
      if (uri && uri->SchemeIs("javascript")) {
        // Step 2.1. Remove the `href` attribute from element.
        return true;
      }
    }
  }

  // Step 3. if element’s element interface is HTMLFormElement, and if element’s
  // action attribute is a URL with "javascript:" protocol:
  if (auto* form = HTMLFormElement::FromNode(aElement)) {
    if (aAttrNamespace == kNameSpaceID_None &&
        aAttrLocalName == nsGkAtoms::action) {
      nsCOMPtr<nsIURI> uri;
      form->GetURIAttr(aAttrLocalName, nullptr, getter_AddRefs(uri));
      if (uri && uri->SchemeIs("javascript")) {
        // Step 3.1 Remove the `action` attribute from element.
        return true;
      }
    }
  }

  // Step 4. if element’s element interface is HTMLInputElement or
  // HTMLButtonElement, and if element’s formaction attribute is a [URL] with
  // javascript: protocol
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::input, nsGkAtoms::button) &&
      aAttrNamespace == kNameSpaceID_None &&
      aAttrLocalName == nsGkAtoms::formaction) {
    // XXX nsGenericHTMLFormControlElementWithState::GetFormAction falls back to
    // the document URI.
    nsGenericHTMLElement* el = nsGenericHTMLElement::FromNode(aElement);
    nsCOMPtr<nsIURI> uri;
    el->GetURIAttr(aAttrLocalName, nullptr, getter_AddRefs(uri));
    if (uri && uri->SchemeIs("javascript")) {
      // Step 4.1 Remove the `formaction` attribute from element.
      return true;
    }
  }

  return false;
}

bool nsTreeSanitizer::SanitizeURL(mozilla::dom::Element* aElement,
                                  int32_t aNamespace, nsAtom* aLocalName,
                                  bool aFragmentsOnly) {
  nsAutoString value;
  aElement->GetAttr(aNamespace, aLocalName, value);

  // Get value and remove mandatory quotes
  static const char* kWhitespace = "\n\r\t\b";
  const nsAString& v = nsContentUtils::TrimCharsInSet(kWhitespace, value);
  // Fragment-only url cannot be harmful.
  if (!v.IsEmpty() && v.First() == u'#') {
    return false;
  }
  // if we allow only same-document fragment URLs, stop and remove here
  if (aFragmentsOnly) {
    aElement->UnsetAttr(aNamespace, aLocalName, false);
    if (mLogRemovals) {
      LogMessage("Removed unsafe URI from element attribute.",
                 aElement->OwnerDoc(), aElement, aLocalName);
    }
    return true;
  }

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  uint32_t flags = nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL;

  nsCOMPtr<nsIURI> attrURI;
  nsresult rv =
      NS_NewURI(getter_AddRefs(attrURI), v, nullptr, aElement->GetBaseURI());
  if (NS_SUCCEEDED(rv)) {
    if (mCidEmbedsOnly && kNameSpaceID_None == aNamespace) {
      if (nsGkAtoms::src == aLocalName || nsGkAtoms::background == aLocalName) {
        // comm-central uses a hack that makes nsIURIs created with cid: specs
        // actually have an about:blank spec. Therefore, nsIURI facilities are
        // useless for cid: when comm-central code is participating.
        if (!(v.Length() > 4 && (v[0] == 'c' || v[0] == 'C') &&
              (v[1] == 'i' || v[1] == 'I') && (v[2] == 'd' || v[2] == 'D') &&
              v[3] == ':')) {
          rv = NS_ERROR_FAILURE;
        }
      } else if (nsGkAtoms::cdgroup_ == aLocalName ||
                 nsGkAtoms::altimg_ == aLocalName ||
                 nsGkAtoms::definitionURL_ == aLocalName) {
        // Gecko doesn't fetch these now and shouldn't in the future, but
        // in case someone goofs with these in the future, let's drop them.
        rv = NS_ERROR_FAILURE;
      } else {
        rv = secMan->CheckLoadURIWithPrincipal(sNullPrincipal, attrURI, flags,
                                               0);
      }
    } else {
      rv = secMan->CheckLoadURIWithPrincipal(sNullPrincipal, attrURI, flags, 0);
    }
  }
  if (NS_FAILED(rv)) {
    aElement->UnsetAttr(aNamespace, aLocalName, false);
    if (mLogRemovals) {
      LogMessage("Removed unsafe URI from element attribute.",
                 aElement->OwnerDoc(), aElement, aLocalName);
    }
    return true;
  }
  return false;
}

void nsTreeSanitizer::Sanitize(DocumentFragment* aFragment) {
  // If you want to relax these preconditions, be sure to check the code in
  // here that notifies / does not notify or that fires mutation events if
  // in tree.
  MOZ_ASSERT(!aFragment->IsInUncomposedDoc(), "The fragment is in doc?");

  mFullDocument = false;
  SanitizeChildren(aFragment);
}

void nsTreeSanitizer::Sanitize(Document* aDocument) {
  // If you want to relax these preconditions, be sure to check the code in
  // here that notifies / does not notify or that fires mutation events if
  // in tree.
#ifdef DEBUG
  MOZ_ASSERT(!aDocument->GetContainer(), "The document is in a shell.");
  RefPtr<mozilla::dom::Element> root = aDocument->GetRootElement();
  MOZ_ASSERT(root->IsHTMLElement(nsGkAtoms::html), "Not HTML root.");
#endif

  mFullDocument = true;
  SanitizeChildren(aDocument);
}

void nsTreeSanitizer::SanitizeChildren(nsINode* aRoot) {
  nsIContent* node = aRoot->GetFirstChild();
  while (node) {
    if (node->IsElement()) {
      mozilla::dom::Element* elt = node->AsElement();
      mozilla::dom::NodeInfo* nodeInfo = node->NodeInfo();
      nsAtom* localName = nodeInfo->NameAtom();
      int32_t ns = nodeInfo->NamespaceID();

      if (MustPrune(ns, localName, elt)) {
        if (mLogRemovals) {
          LogMessage("Removing unsafe node.", elt->OwnerDoc(), elt);
        }
        RemoveAllAttributes(elt);
        nsIContent* descendant = node;
        while ((descendant = descendant->GetNextNode(node))) {
          if (descendant->IsElement()) {
            RemoveAllAttributes(descendant->AsElement());
          }
        }
        nsIContent* next = node->GetNextNonChildNode(aRoot);
        node->RemoveFromParent();
        node = next;
        continue;
      }
      if (auto* templateEl = HTMLTemplateElement::FromNode(elt)) {
        // traverse into the DocFragment content attribute of template elements
        bool wasFullDocument = mFullDocument;
        mFullDocument = false;
        RefPtr<DocumentFragment> frag = templateEl->Content();
        SanitizeChildren(frag);
        mFullDocument = wasFullDocument;
      }
      if (!mIsForSanitizerAPI && nsGkAtoms::style == localName) {
        // If styles aren't allowed, style elements got pruned above. Even
        // if styles are allowed, non-HTML, non-SVG style elements got pruned
        // above.
        NS_ASSERTION(ns == kNameSpaceID_XHTML || ns == kNameSpaceID_SVG,
                     "Should have only HTML or SVG here!");
        if (SanitizeInlineStyle(elt, StyleSanitizationKind::Standard) &&
            mLogRemovals) {
          LogMessage("Removed some rules and/or properties from stylesheet.",
                     aRoot->OwnerDoc());
        }

        AllowedAttributes allowed;
        allowed.mStyle = mAllowStyles;
        if (ns == kNameSpaceID_XHTML) {
          allowed.mNames = sAttributesHTML;
          allowed.mURLs = kURLAttributesHTML;
        } else {
          allowed.mNames = sAttributesSVG;
          allowed.mURLs = kURLAttributesSVG;
          allowed.mXLink = true;
        }
        SanitizeAttributes(elt, allowed);
        node = node->GetNextNonChildNode(aRoot);
        continue;
      }
      if (MustFlatten(ns, localName)) {
        if (mLogRemovals) {
          LogMessage("Flattening unsafe node (descendants are preserved).",
                     elt->OwnerDoc(), elt);
        }
        RemoveAllAttributes(elt);
        nsCOMPtr<nsIContent> next = node->GetNextNode(aRoot);
        nsCOMPtr<nsIContent> parent = node->GetParent();
        nsCOMPtr<nsIContent> child;  // Must keep the child alive during move
        ErrorResult rv;
        while ((child = node->GetFirstChild())) {
          nsCOMPtr<nsINode> refNode = node;
          parent->InsertBefore(*child, refNode, rv);
          if (rv.Failed()) {
            break;
          }
        }
        node->RemoveFromParent();
        node = next;
        continue;
      }
      NS_ASSERTION(ns == kNameSpaceID_XHTML || ns == kNameSpaceID_SVG ||
                       ns == kNameSpaceID_MathML,
                   "Should have only HTML, MathML or SVG here!");
      AllowedAttributes allowed;
      if (ns == kNameSpaceID_XHTML) {
        allowed.mNames = sAttributesHTML;
        allowed.mURLs = kURLAttributesHTML;
        allowed.mStyle = mAllowStyles;
        allowed.mDangerousSrc = nsGkAtoms::img == localName && !mCidEmbedsOnly;
        SanitizeAttributes(elt, allowed);
      } else if (ns == kNameSpaceID_SVG) {
        allowed.mNames = sAttributesSVG;
        allowed.mURLs = kURLAttributesSVG;
        allowed.mXLink = true;
        allowed.mStyle = mAllowStyles;
        SanitizeAttributes(elt, allowed);
      } else {
        allowed.mNames = sAttributesMathML;
        allowed.mURLs = kURLAttributesMathML;
        allowed.mXLink = true;
        SanitizeAttributes(elt, allowed);
      }
      node = node->GetNextNode(aRoot);
      continue;
    }
    NS_ASSERTION(!node->GetFirstChild(), "How come non-element node had kids?");
    nsIContent* next = node->GetNextNonChildNode(aRoot);
    if (!mAllowComments && node->IsComment()) {
      node->RemoveFromParent();
    }
    node = next;
  }
}

void nsTreeSanitizer::RemoveAllAttributes(Element* aElement) {
  const nsAttrName* attrName;
  while ((attrName = aElement->GetAttrNameAt(0))) {
    int32_t attrNs = attrName->NamespaceID();
    RefPtr<nsAtom> attrLocal = attrName->LocalName();
    aElement->UnsetAttr(attrNs, attrLocal, false);
  }
}

void nsTreeSanitizer::RemoveAllAttributesFromDescendants(
    mozilla::dom::Element* aElement) {
  nsIContent* node = aElement->GetFirstChild();
  while (node) {
    if (node->IsElement()) {
      mozilla::dom::Element* elt = node->AsElement();
      RemoveAllAttributes(elt);
    }
    node = node->GetNextNode(aElement);
  }
}

void nsTreeSanitizer::LogMessage(const char* aMessage, Document* aDoc,
                                 Element* aElement, nsAtom* aAttr) {
  if (mLogRemovals) {
    nsAutoString msg;
    msg.Assign(NS_ConvertASCIItoUTF16(aMessage));
    if (aElement) {
      msg.Append(u" Element: "_ns + aElement->LocalName() + u"."_ns);
    }
    if (aAttr) {
      msg.Append(u" Attribute: "_ns + nsDependentAtomString(aAttr) + u"."_ns);
    }

    if (mInnerWindowID) {
      nsContentUtils::ReportToConsoleByWindowID(
          msg, nsIScriptError::warningFlag, "DOM"_ns, mInnerWindowID);
    } else {
      nsContentUtils::ReportToConsoleNonLocalized(
          msg, nsIScriptError::warningFlag, "DOM"_ns, aDoc);
    }
  }
}

void nsTreeSanitizer::InitializeStatics() {
  MOZ_ASSERT(!sElementsHTML, "Initializing a second time.");

  sElementsHTML = new AtomsTable(ArrayLength(kElementsHTML));
  for (uint32_t i = 0; kElementsHTML[i]; i++) {
    sElementsHTML->Insert(kElementsHTML[i]);
  }

  sAttributesHTML = new AtomsTable(ArrayLength(kAttributesHTML));
  for (uint32_t i = 0; kAttributesHTML[i]; i++) {
    sAttributesHTML->Insert(kAttributesHTML[i]);
  }

  sPresAttributesHTML = new AtomsTable(ArrayLength(kPresAttributesHTML));
  for (uint32_t i = 0; kPresAttributesHTML[i]; i++) {
    sPresAttributesHTML->Insert(kPresAttributesHTML[i]);
  }

  sElementsSVG = new AtomsTable(ArrayLength(kElementsSVG));
  for (uint32_t i = 0; kElementsSVG[i]; i++) {
    sElementsSVG->Insert(kElementsSVG[i]);
  }

  sAttributesSVG = new AtomsTable(ArrayLength(kAttributesSVG));
  for (uint32_t i = 0; kAttributesSVG[i]; i++) {
    sAttributesSVG->Insert(kAttributesSVG[i]);
  }

  sElementsMathML = new AtomsTable(ArrayLength(kElementsMathML));
  for (uint32_t i = 0; kElementsMathML[i]; i++) {
    sElementsMathML->Insert(kElementsMathML[i]);
  }

  sAttributesMathML = new AtomsTable(ArrayLength(kAttributesMathML));
  for (uint32_t i = 0; kAttributesMathML[i]; i++) {
    sAttributesMathML->Insert(kAttributesMathML[i]);
  }

  sBaselineAttributeAllowlist =
      new AtomsTable(ArrayLength(kBaselineAttributeAllowlist));
  for (const auto* atom : kBaselineAttributeAllowlist) {
    sBaselineAttributeAllowlist->Insert(atom);
  }

  sBaselineElementAllowlist =
      new AtomsTable(ArrayLength(kBaselineElementAllowlist));
  for (const auto* atom : kBaselineElementAllowlist) {
    sBaselineElementAllowlist->Insert(atom);
  }

  sDefaultConfigurationAttributeAllowlist =
      new AtomsTable(ArrayLength(kDefaultConfigurationAttributeAllowlist));
  for (const auto* atom : kDefaultConfigurationAttributeAllowlist) {
    sDefaultConfigurationAttributeAllowlist->Insert(atom);
  }

  sDefaultConfigurationElementAllowlist =
      new AtomsTable(ArrayLength(kDefaultConfigurationElementAllowlist));
  for (const auto* atom : kDefaultConfigurationElementAllowlist) {
    sDefaultConfigurationElementAllowlist->Insert(atom);
  }

  nsCOMPtr<nsIPrincipal> principal =
      NullPrincipal::CreateWithoutOriginAttributes();
  principal.forget(&sNullPrincipal);
}

void nsTreeSanitizer::ReleaseStatics() {
  delete sElementsHTML;
  sElementsHTML = nullptr;

  delete sAttributesHTML;
  sAttributesHTML = nullptr;

  delete sPresAttributesHTML;
  sPresAttributesHTML = nullptr;

  delete sElementsSVG;
  sElementsSVG = nullptr;

  delete sAttributesSVG;
  sAttributesSVG = nullptr;

  delete sElementsMathML;
  sElementsMathML = nullptr;

  delete sAttributesMathML;
  sAttributesMathML = nullptr;

  delete sBaselineAttributeAllowlist;
  sBaselineAttributeAllowlist = nullptr;

  delete sBaselineElementAllowlist;
  sBaselineElementAllowlist = nullptr;

  delete sDefaultConfigurationAttributeAllowlist;
  sDefaultConfigurationAttributeAllowlist = nullptr;

  delete sDefaultConfigurationElementAllowlist;
  sDefaultConfigurationElementAllowlist = nullptr;

  NS_IF_RELEASE(sNullPrincipal);
}

static int32_t ConvertNamespaceString(const nsAString& aNamespace,
                                      bool aForAttribute,
                                      mozilla::ErrorResult& aRv) {
  int32_t namespaceID = nsNameSpaceManager::GetInstance()->GetNameSpaceID(
      aNamespace, /* aInChromeDoc */ false);
  if (namespaceID == kNameSpaceID_XHTML || namespaceID == kNameSpaceID_MathML ||
      namespaceID == kNameSpaceID_SVG) {
    return namespaceID;
  }
  if (aForAttribute && (namespaceID == kNameSpaceID_XMLNS ||
                        namespaceID == kNameSpaceID_XLink)) {
    return namespaceID;
  }

  aRv.ThrowTypeError("Invalid namespace: \""_ns +
                     NS_ConvertUTF16toUTF8(aNamespace) + "\"."_ns);
  return kNameSpaceID_Unknown;
}

UniquePtr<nsTreeSanitizer::ElementNameSet> nsTreeSanitizer::ConvertElements(
    const nsTArray<OwningStringOrSanitizerElementNamespace>& aElements,
    mozilla::ErrorResult& aRv) {
  auto set = MakeUnique<ElementNameSet>(aElements.Length());
  for (const auto& entry : aElements) {
    if (entry.IsString()) {
      RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(entry.GetAsString());
      // The default namespace for elements is HTML.
      ElementName elemName(kNameSpaceID_XHTML, std::move(nameAtom));
      set->Insert(elemName);
    } else {
      const auto& elemNamespace = entry.GetAsSanitizerElementNamespace();

      int32_t namespaceID =
          ConvertNamespaceString(elemNamespace.mNamespace, false, aRv);
      if (aRv.Failed()) {
        return {};
      }

      RefPtr<nsAtom> nameAtom = NS_AtomizeMainThread(elemNamespace.mName);
      ElementName elemName(namespaceID, std::move(nameAtom));
      set->Insert(elemName);
    }
  }

  return set;
}

UniquePtr<nsTreeSanitizer::ElementNameSet> nsTreeSanitizer::ConvertElements(
    const OwningStarOrStringOrSanitizerElementNamespaceSequence& aElements,
    mozilla::ErrorResult& aRv) {
  if (aElements.IsStar()) {
    return nullptr;
  }
  return ConvertElements(
      aElements.GetAsStringOrSanitizerElementNamespaceSequence(), aRv);
}

UniquePtr<nsTreeSanitizer::AttributesToElementsMap>
nsTreeSanitizer::ConvertAttributes(
    const nsTArray<SanitizerAttribute>& aAttributes, ErrorResult& aRv) {
  auto map = MakeUnique<AttributesToElementsMap>();

  for (const auto& entry : aAttributes) {
    // The default namespace for attributes is the "null" namespace.
    int32_t namespaceID = kNameSpaceID_None;
    if (!entry.mNamespace.IsVoid()) {
      namespaceID = ConvertNamespaceString(entry.mNamespace, true, aRv);
      if (aRv.Failed()) {
        return {};
      }
    }
    RefPtr<nsAtom> attrAtom = NS_AtomizeMainThread(entry.mName);
    AttributeName attrName(namespaceID, std::move(attrAtom));

    UniquePtr<ElementNameSet> elements = ConvertElements(entry.mElements, aRv);
    if (aRv.Failed()) {
      return {};
    }
    map->InsertOrUpdate(attrName, std::move(elements));
  }

  return map;
}

void nsTreeSanitizer::WithWebSanitizerOptions(
    nsIGlobalObject* aGlobal, const mozilla::dom::SanitizerConfig& aOptions,
    ErrorResult& aRv) {
  if (StaticPrefs::dom_security_sanitizer_logging()) {
    mLogRemovals = true;
    if (nsPIDOMWindowInner* win = aGlobal->GetAsInnerWindow()) {
      mInnerWindowID = win->WindowID();
    }
  }

  mIsForSanitizerAPI = true;

  if (aOptions.mAllowComments.WasPassed()) {
    mAllowComments = aOptions.mAllowComments.Value();
  }
  if (aOptions.mAllowCustomElements.WasPassed()) {
    mAllowCustomElements = aOptions.mAllowCustomElements.Value();
  }
  if (aOptions.mAllowUnknownMarkup.WasPassed()) {
    mAllowUnknownMarkup = aOptions.mAllowUnknownMarkup.Value();
  }

  if (aOptions.mAllowElements.WasPassed()) {
    mAllowElements = ConvertElements(aOptions.mAllowElements.Value(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  if (aOptions.mBlockElements.WasPassed()) {
    mBlockElements = ConvertElements(aOptions.mBlockElements.Value(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  if (aOptions.mDropElements.WasPassed()) {
    mDropElements = ConvertElements(aOptions.mDropElements.Value(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  if (aOptions.mAllowAttributes.WasPassed()) {
    mAllowAttributes =
        ConvertAttributes(aOptions.mAllowAttributes.Value(), aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  if (aOptions.mDropAttributes.WasPassed()) {
    mDropAttributes = ConvertAttributes(aOptions.mDropAttributes.Value(), aRv);
  }
}
