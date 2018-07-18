/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeSanitizer.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/BindingStyleRule.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Rule.h"
#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/NullPrincipal.h"
#include "nsCSSPropertyID.h"
#include "nsUnicharInputStream.h"
#include "nsAttrName.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIParserUtils.h"
#include "nsIDocument.h"
#include "nsQueryObject.h"

using namespace mozilla;
using namespace mozilla::dom;

//
// Thanks to Mark Pilgrim and Sam Ruby for the initial whitelist
//
const nsStaticAtom* const kElementsHTML[] = {
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
  nsGkAtoms::datalist,
  nsGkAtoms::dd,
  nsGkAtoms::del,
  nsGkAtoms::details,
  nsGkAtoms::dfn,
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
  nsGkAtoms::label,
  nsGkAtoms::legend,
  nsGkAtoms::li,
  nsGkAtoms::link,
  nsGkAtoms::listing,
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
};

const nsStaticAtom* const kAttributesHTML[] = {
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
};

const nsStaticAtom* const kPresAttributesHTML[] = {
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
};

const nsStaticAtom* const kURLAttributesHTML[] = {
  nsGkAtoms::action,
  nsGkAtoms::href,
  nsGkAtoms::src,
  nsGkAtoms::longdesc,
  nsGkAtoms::cite,
  nsGkAtoms::background,
  nullptr
};

const nsStaticAtom* const kElementsSVG[] = {
  nsGkAtoms::a, // a
  nsGkAtoms::circle, // circle
  nsGkAtoms::clipPath, // clipPath
  nsGkAtoms::colorProfile, // color-profile
  nsGkAtoms::cursor, // cursor
  nsGkAtoms::defs, // defs
  nsGkAtoms::desc, // desc
  nsGkAtoms::ellipse, // ellipse
  nsGkAtoms::elevation, // elevation
  nsGkAtoms::erode, // erode
  nsGkAtoms::ex, // ex
  nsGkAtoms::exact, // exact
  nsGkAtoms::exponent, // exponent
  nsGkAtoms::feBlend, // feBlend
  nsGkAtoms::feColorMatrix, // feColorMatrix
  nsGkAtoms::feComponentTransfer, // feComponentTransfer
  nsGkAtoms::feComposite, // feComposite
  nsGkAtoms::feConvolveMatrix, // feConvolveMatrix
  nsGkAtoms::feDiffuseLighting, // feDiffuseLighting
  nsGkAtoms::feDisplacementMap, // feDisplacementMap
  nsGkAtoms::feDistantLight, // feDistantLight
  nsGkAtoms::feDropShadow, // feDropShadow
  nsGkAtoms::feFlood, // feFlood
  nsGkAtoms::feFuncA, // feFuncA
  nsGkAtoms::feFuncB, // feFuncB
  nsGkAtoms::feFuncG, // feFuncG
  nsGkAtoms::feFuncR, // feFuncR
  nsGkAtoms::feGaussianBlur, // feGaussianBlur
  nsGkAtoms::feImage, // feImage
  nsGkAtoms::feMerge, // feMerge
  nsGkAtoms::feMergeNode, // feMergeNode
  nsGkAtoms::feMorphology, // feMorphology
  nsGkAtoms::feOffset, // feOffset
  nsGkAtoms::fePointLight, // fePointLight
  nsGkAtoms::feSpecularLighting, // feSpecularLighting
  nsGkAtoms::feSpotLight, // feSpotLight
  nsGkAtoms::feTile, // feTile
  nsGkAtoms::feTurbulence, // feTurbulence
  nsGkAtoms::filter, // filter
  nsGkAtoms::font, // font
  nsGkAtoms::font_face, // font-face
  nsGkAtoms::font_face_format, // font-face-format
  nsGkAtoms::font_face_name, // font-face-name
  nsGkAtoms::font_face_src, // font-face-src
  nsGkAtoms::font_face_uri, // font-face-uri
  nsGkAtoms::foreignObject, // foreignObject
  nsGkAtoms::g, // g
  // glyph
  nsGkAtoms::glyphRef, // glyphRef
  // hkern
  nsGkAtoms::image, // image
  nsGkAtoms::line, // line
  nsGkAtoms::linearGradient, // linearGradient
  nsGkAtoms::marker, // marker
  nsGkAtoms::mask, // mask
  nsGkAtoms::metadata, // metadata
  nsGkAtoms::missingGlyph, // missingGlyph
  nsGkAtoms::mpath, // mpath
  nsGkAtoms::path, // path
  nsGkAtoms::pattern, // pattern
  nsGkAtoms::polygon, // polygon
  nsGkAtoms::polyline, // polyline
  nsGkAtoms::radialGradient, // radialGradient
  nsGkAtoms::rect, // rect
  nsGkAtoms::stop, // stop
  nsGkAtoms::svg, // svg
  nsGkAtoms::svgSwitch, // switch
  nsGkAtoms::symbol, // symbol
  nsGkAtoms::text, // text
  nsGkAtoms::textPath, // textPath
  nsGkAtoms::title, // title
  nsGkAtoms::tref, // tref
  nsGkAtoms::tspan, // tspan
  nsGkAtoms::use, // use
  nsGkAtoms::view, // view
  // vkern
  nullptr
};

const nsStaticAtom* const kAttributesSVG[] = {
  // accent-height
  nsGkAtoms::accumulate, // accumulate
  nsGkAtoms::additive, // additive
  nsGkAtoms::alignment_baseline, // alignment-baseline
  // alphabetic
  nsGkAtoms::amplitude, // amplitude
  // arabic-form
  // ascent
  nsGkAtoms::attributeName, // attributeName
  nsGkAtoms::attributeType, // attributeType
  nsGkAtoms::azimuth, // azimuth
  nsGkAtoms::baseFrequency, // baseFrequency
  nsGkAtoms::baseline_shift, // baseline-shift
  // baseProfile
  // bbox
  nsGkAtoms::begin, // begin
  nsGkAtoms::bias, // bias
  nsGkAtoms::by, // by
  nsGkAtoms::calcMode, // calcMode
  // cap-height
  nsGkAtoms::_class, // class
  nsGkAtoms::clip_path, // clip-path
  nsGkAtoms::clip_rule, // clip-rule
  nsGkAtoms::clipPathUnits, // clipPathUnits
  nsGkAtoms::color, // color
  nsGkAtoms::colorInterpolation, // color-interpolation
  nsGkAtoms::colorInterpolationFilters, // color-interpolation-filters
  nsGkAtoms::cursor, // cursor
  nsGkAtoms::cx, // cx
  nsGkAtoms::cy, // cy
  nsGkAtoms::d, // d
  // descent
  nsGkAtoms::diffuseConstant, // diffuseConstant
  nsGkAtoms::direction, // direction
  nsGkAtoms::display, // display
  nsGkAtoms::divisor, // divisor
  nsGkAtoms::dominant_baseline, // dominant-baseline
  nsGkAtoms::dur, // dur
  nsGkAtoms::dx, // dx
  nsGkAtoms::dy, // dy
  nsGkAtoms::edgeMode, // edgeMode
  nsGkAtoms::elevation, // elevation
  // enable-background
  nsGkAtoms::end, // end
  nsGkAtoms::fill, // fill
  nsGkAtoms::fill_opacity, // fill-opacity
  nsGkAtoms::fill_rule, // fill-rule
  nsGkAtoms::filter, // filter
  nsGkAtoms::filterUnits, // filterUnits
  nsGkAtoms::flood_color, // flood-color
  nsGkAtoms::flood_opacity, // flood-opacity
  // XXX focusable
  nsGkAtoms::font, // font
  nsGkAtoms::font_family, // font-family
  nsGkAtoms::font_size, // font-size
  nsGkAtoms::font_size_adjust, // font-size-adjust
  nsGkAtoms::font_stretch, // font-stretch
  nsGkAtoms::font_style, // font-style
  nsGkAtoms::font_variant, // font-variant
  nsGkAtoms::fontWeight, // font-weight
  nsGkAtoms::format, // format
  nsGkAtoms::from, // from
  nsGkAtoms::fx, // fx
  nsGkAtoms::fy, // fy
  // g1
  // g2
  // glyph-name
  // glyphRef
  // glyph-orientation-horizontal
  // glyph-orientation-vertical
  nsGkAtoms::gradientTransform, // gradientTransform
  nsGkAtoms::gradientUnits, // gradientUnits
  nsGkAtoms::height, // height
  // horiz-adv-x
  // horiz-origin-x
  // horiz-origin-y
  nsGkAtoms::id, // id
  // ideographic
  nsGkAtoms::image_rendering, // image-rendering
  nsGkAtoms::in, // in
  nsGkAtoms::in2, // in2
  nsGkAtoms::intercept, // intercept
  // k
  nsGkAtoms::k1, // k1
  nsGkAtoms::k2, // k2
  nsGkAtoms::k3, // k3
  nsGkAtoms::k4, // k4
  // kerning
  nsGkAtoms::kernelMatrix, // kernelMatrix
  nsGkAtoms::kernelUnitLength, // kernelUnitLength
  nsGkAtoms::keyPoints, // keyPoints
  nsGkAtoms::keySplines, // keySplines
  nsGkAtoms::keyTimes, // keyTimes
  nsGkAtoms::lang, // lang
  // lengthAdjust
  nsGkAtoms::letter_spacing, // letter-spacing
  nsGkAtoms::lighting_color, // lighting-color
  nsGkAtoms::limitingConeAngle, // limitingConeAngle
  // local
  nsGkAtoms::marker, // marker
  nsGkAtoms::marker_end, // marker-end
  nsGkAtoms::marker_mid, // marker-mid
  nsGkAtoms::marker_start, // marker-start
  nsGkAtoms::markerHeight, // markerHeight
  nsGkAtoms::markerUnits, // markerUnits
  nsGkAtoms::markerWidth, // markerWidth
  nsGkAtoms::mask, // mask
  nsGkAtoms::maskContentUnits, // maskContentUnits
  nsGkAtoms::maskUnits, // maskUnits
  // mathematical
  nsGkAtoms::max, // max
  nsGkAtoms::media, // media
  nsGkAtoms::method, // method
  nsGkAtoms::min, // min
  nsGkAtoms::mode, // mode
  nsGkAtoms::name, // name
  nsGkAtoms::numOctaves, // numOctaves
  nsGkAtoms::offset, // offset
  nsGkAtoms::opacity, // opacity
  nsGkAtoms::_operator, // operator
  nsGkAtoms::order, // order
  nsGkAtoms::orient, // orient
  nsGkAtoms::orientation, // orientation
  // origin
  // overline-position
  // overline-thickness
  nsGkAtoms::overflow, // overflow
  // panose-1
  nsGkAtoms::path, // path
  nsGkAtoms::pathLength, // pathLength
  nsGkAtoms::patternContentUnits, // patternContentUnits
  nsGkAtoms::patternTransform, // patternTransform
  nsGkAtoms::patternUnits, // patternUnits
  nsGkAtoms::pointer_events, // pointer-events XXX is this safe?
  nsGkAtoms::points, // points
  nsGkAtoms::pointsAtX, // pointsAtX
  nsGkAtoms::pointsAtY, // pointsAtY
  nsGkAtoms::pointsAtZ, // pointsAtZ
  nsGkAtoms::preserveAlpha, // preserveAlpha
  nsGkAtoms::preserveAspectRatio, // preserveAspectRatio
  nsGkAtoms::primitiveUnits, // primitiveUnits
  nsGkAtoms::r, // r
  nsGkAtoms::radius, // radius
  nsGkAtoms::refX, // refX
  nsGkAtoms::refY, // refY
  nsGkAtoms::repeatCount, // repeatCount
  nsGkAtoms::repeatDur, // repeatDur
  nsGkAtoms::requiredExtensions, // requiredExtensions
  nsGkAtoms::requiredFeatures, // requiredFeatures
  nsGkAtoms::restart, // restart
  nsGkAtoms::result, // result
  nsGkAtoms::rotate, // rotate
  nsGkAtoms::rx, // rx
  nsGkAtoms::ry, // ry
  nsGkAtoms::scale, // scale
  nsGkAtoms::seed, // seed
  nsGkAtoms::shape_rendering, // shape-rendering
  nsGkAtoms::slope, // slope
  nsGkAtoms::spacing, // spacing
  nsGkAtoms::specularConstant, // specularConstant
  nsGkAtoms::specularExponent, // specularExponent
  nsGkAtoms::spreadMethod, // spreadMethod
  nsGkAtoms::startOffset, // startOffset
  nsGkAtoms::stdDeviation, // stdDeviation
  // stemh
  // stemv
  nsGkAtoms::stitchTiles, // stitchTiles
  nsGkAtoms::stop_color, // stop-color
  nsGkAtoms::stop_opacity, // stop-opacity
  // strikethrough-position
  // strikethrough-thickness
  nsGkAtoms::string, // string
  nsGkAtoms::stroke, // stroke
  nsGkAtoms::stroke_dasharray, // stroke-dasharray
  nsGkAtoms::stroke_dashoffset, // stroke-dashoffset
  nsGkAtoms::stroke_linecap, // stroke-linecap
  nsGkAtoms::stroke_linejoin, // stroke-linejoin
  nsGkAtoms::stroke_miterlimit, // stroke-miterlimit
  nsGkAtoms::stroke_opacity, // stroke-opacity
  nsGkAtoms::stroke_width, // stroke-width
  nsGkAtoms::surfaceScale, // surfaceScale
  nsGkAtoms::systemLanguage, // systemLanguage
  nsGkAtoms::tableValues, // tableValues
  nsGkAtoms::target, // target
  nsGkAtoms::targetX, // targetX
  nsGkAtoms::targetY, // targetY
  nsGkAtoms::text_anchor, // text-anchor
  nsGkAtoms::text_decoration, // text-decoration
  // textLength
  nsGkAtoms::text_rendering, // text-rendering
  nsGkAtoms::title, // title
  nsGkAtoms::to, // to
  nsGkAtoms::transform, // transform
  nsGkAtoms::type, // type
  // u1
  // u2
  // underline-position
  // underline-thickness
  // unicode
  nsGkAtoms::unicode_bidi, // unicode-bidi
  // unicode-range
  // units-per-em
  // v-alphabetic
  // v-hanging
  // v-ideographic
  // v-mathematical
  nsGkAtoms::values, // values
  nsGkAtoms::vector_effect, // vector-effect
  // vert-adv-y
  // vert-origin-x
  // vert-origin-y
  nsGkAtoms::viewBox, // viewBox
  nsGkAtoms::viewTarget, // viewTarget
  nsGkAtoms::visibility, // visibility
  nsGkAtoms::width, // width
  // widths
  nsGkAtoms::word_spacing, // word-spacing
  nsGkAtoms::writing_mode, // writing-mode
  nsGkAtoms::x, // x
  // x-height
  nsGkAtoms::x1, // x1
  nsGkAtoms::x2, // x2
  nsGkAtoms::xChannelSelector, // xChannelSelector
  nsGkAtoms::y, // y
  nsGkAtoms::y1, // y1
  nsGkAtoms::y2, // y2
  nsGkAtoms::yChannelSelector, // yChannelSelector
  nsGkAtoms::z, // z
  nsGkAtoms::zoomAndPan, // zoomAndPan
  nullptr
};

const nsStaticAtom* const kURLAttributesSVG[] = {
  nsGkAtoms::href,
  nullptr
};

const nsStaticAtom* const kElementsMathML[] = {
   nsGkAtoms::abs_, // abs
   nsGkAtoms::_and, // and
   nsGkAtoms::annotation_, // annotation
   nsGkAtoms::annotation_xml_, // annotation-xml
   nsGkAtoms::apply_, // apply
   nsGkAtoms::approx_, // approx
   nsGkAtoms::arccos_, // arccos
   nsGkAtoms::arccosh_, // arccosh
   nsGkAtoms::arccot_, // arccot
   nsGkAtoms::arccoth_, // arccoth
   nsGkAtoms::arccsc_, // arccsc
   nsGkAtoms::arccsch_, // arccsch
   nsGkAtoms::arcsec_, // arcsec
   nsGkAtoms::arcsech_, // arcsech
   nsGkAtoms::arcsin_, // arcsin
   nsGkAtoms::arcsinh_, // arcsinh
   nsGkAtoms::arctan_, // arctan
   nsGkAtoms::arctanh_, // arctanh
   nsGkAtoms::arg_, // arg
   nsGkAtoms::bind_, // bind
   nsGkAtoms::bvar_, // bvar
   nsGkAtoms::card_, // card
   nsGkAtoms::cartesianproduct_, // cartesianproduct
   nsGkAtoms::cbytes_, // cbytes
   nsGkAtoms::ceiling, // ceiling
   nsGkAtoms::cerror_, // cerror
   nsGkAtoms::ci_, // ci
   nsGkAtoms::cn_, // cn
   nsGkAtoms::codomain_, // codomain
   nsGkAtoms::complexes_, // complexes
   nsGkAtoms::compose_, // compose
   nsGkAtoms::condition_, // condition
   nsGkAtoms::conjugate_, // conjugate
   nsGkAtoms::cos_, // cos
   nsGkAtoms::cosh_, // cosh
   nsGkAtoms::cot_, // cot
   nsGkAtoms::coth_, // coth
   nsGkAtoms::cs_, // cs
   nsGkAtoms::csc_, // csc
   nsGkAtoms::csch_, // csch
   nsGkAtoms::csymbol_, // csymbol
   nsGkAtoms::curl_, // curl
   nsGkAtoms::declare, // declare
   nsGkAtoms::degree_, // degree
   nsGkAtoms::determinant_, // determinant
   nsGkAtoms::diff_, // diff
   nsGkAtoms::divergence_, // divergence
   nsGkAtoms::divide_, // divide
   nsGkAtoms::domain_, // domain
   nsGkAtoms::domainofapplication_, // domainofapplication
   nsGkAtoms::el, // el
   nsGkAtoms::emptyset_, // emptyset
   nsGkAtoms::eq_, // eq
   nsGkAtoms::equivalent_, // equivalent
   nsGkAtoms::eulergamma_, // eulergamma
   nsGkAtoms::exists_, // exists
   nsGkAtoms::exp_, // exp
   nsGkAtoms::exponentiale_, // exponentiale
   nsGkAtoms::factorial_, // factorial
   nsGkAtoms::factorof_, // factorof
   nsGkAtoms::_false, // false
   nsGkAtoms::floor, // floor
   nsGkAtoms::fn_, // fn
   nsGkAtoms::forall_, // forall
   nsGkAtoms::gcd_, // gcd
   nsGkAtoms::geq_, // geq
   nsGkAtoms::grad, // grad
   nsGkAtoms::gt_, // gt
   nsGkAtoms::ident_, // ident
   nsGkAtoms::image, // image
   nsGkAtoms::imaginary_, // imaginary
   nsGkAtoms::imaginaryi_, // imaginaryi
   nsGkAtoms::implies_, // implies
   nsGkAtoms::in, // in
   nsGkAtoms::infinity, // infinity
   nsGkAtoms::int_, // int
   nsGkAtoms::integers_, // integers
   nsGkAtoms::intersect_, // intersect
   nsGkAtoms::interval_, // interval
   nsGkAtoms::inverse_, // inverse
   nsGkAtoms::lambda_, // lambda
   nsGkAtoms::laplacian_, // laplacian
   nsGkAtoms::lcm_, // lcm
   nsGkAtoms::leq_, // leq
   nsGkAtoms::limit_, // limit
   nsGkAtoms::list_, // list
   nsGkAtoms::ln_, // ln
   nsGkAtoms::log_, // log
   nsGkAtoms::logbase_, // logbase
   nsGkAtoms::lowlimit_, // lowlimit
   nsGkAtoms::lt_, // lt
   nsGkAtoms::maction_, // maction
   nsGkAtoms::maligngroup_, // maligngroup
   nsGkAtoms::malignmark_, // malignmark
   nsGkAtoms::math, // math
   nsGkAtoms::matrix, // matrix
   nsGkAtoms::matrixrow_, // matrixrow
   nsGkAtoms::max, // max
   nsGkAtoms::mean_, // mean
   nsGkAtoms::median_, // median
   nsGkAtoms::menclose_, // menclose
   nsGkAtoms::merror_, // merror
   nsGkAtoms::mfenced_, // mfenced
   nsGkAtoms::mfrac_, // mfrac
   nsGkAtoms::mglyph_, // mglyph
   nsGkAtoms::mi_, // mi
   nsGkAtoms::min, // min
   nsGkAtoms::minus_, // minus
   nsGkAtoms::mlabeledtr_, // mlabeledtr
   nsGkAtoms::mlongdiv_, // mlongdiv
   nsGkAtoms::mmultiscripts_, // mmultiscripts
   nsGkAtoms::mn_, // mn
   nsGkAtoms::mo_, // mo
   nsGkAtoms::mode, // mode
   nsGkAtoms::moment_, // moment
   nsGkAtoms::momentabout_, // momentabout
   nsGkAtoms::mover_, // mover
   nsGkAtoms::mpadded_, // mpadded
   nsGkAtoms::mphantom_, // mphantom
   nsGkAtoms::mprescripts_, // mprescripts
   nsGkAtoms::mroot_, // mroot
   nsGkAtoms::mrow_, // mrow
   nsGkAtoms::ms_, // ms
   nsGkAtoms::mscarries_, // mscarries
   nsGkAtoms::mscarry_, // mscarry
   nsGkAtoms::msgroup_, // msgroup
   nsGkAtoms::msline_, // msline
   nsGkAtoms::mspace_, // mspace
   nsGkAtoms::msqrt_, // msqrt
   nsGkAtoms::msrow_, // msrow
   nsGkAtoms::mstack_, // mstack
   nsGkAtoms::mstyle_, // mstyle
   nsGkAtoms::msub_, // msub
   nsGkAtoms::msubsup_, // msubsup
   nsGkAtoms::msup_, // msup
   nsGkAtoms::mtable_, // mtable
   nsGkAtoms::mtd_, // mtd
   nsGkAtoms::mtext_, // mtext
   nsGkAtoms::mtr_, // mtr
   nsGkAtoms::munder_, // munder
   nsGkAtoms::munderover_, // munderover
   nsGkAtoms::naturalnumbers_, // naturalnumbers
   nsGkAtoms::neq_, // neq
   nsGkAtoms::none, // none
   nsGkAtoms::_not, // not
   nsGkAtoms::notanumber_, // notanumber
   nsGkAtoms::note_, // note
   nsGkAtoms::notin_, // notin
   nsGkAtoms::notprsubset_, // notprsubset
   nsGkAtoms::notsubset_, // notsubset
   nsGkAtoms::_or, // or
   nsGkAtoms::otherwise, // otherwise
   nsGkAtoms::outerproduct_, // outerproduct
   nsGkAtoms::partialdiff_, // partialdiff
   nsGkAtoms::pi_, // pi
   nsGkAtoms::piece_, // piece
   nsGkAtoms::piecewise_, // piecewise
   nsGkAtoms::plus_, // plus
   nsGkAtoms::power_, // power
   nsGkAtoms::primes_, // primes
   nsGkAtoms::product_, // product
   nsGkAtoms::prsubset_, // prsubset
   nsGkAtoms::quotient_, // quotient
   nsGkAtoms::rationals_, // rationals
   nsGkAtoms::real_, // real
   nsGkAtoms::reals_, // reals
   nsGkAtoms::reln_, // reln
   nsGkAtoms::rem, // rem
   nsGkAtoms::root_, // root
   nsGkAtoms::scalarproduct_, // scalarproduct
   nsGkAtoms::sdev_, // sdev
   nsGkAtoms::sec_, // sec
   nsGkAtoms::sech_, // sech
   nsGkAtoms::selector_, // selector
   nsGkAtoms::semantics_, // semantics
   nsGkAtoms::sep_, // sep
   nsGkAtoms::set, // set
   nsGkAtoms::setdiff_, // setdiff
   nsGkAtoms::share_, // share
   nsGkAtoms::sin_, // sin
   nsGkAtoms::sinh_, // sinh
   nsGkAtoms::subset_, // subset
   nsGkAtoms::sum, // sum
   nsGkAtoms::tan_, // tan
   nsGkAtoms::tanh_, // tanh
   nsGkAtoms::tendsto_, // tendsto
   nsGkAtoms::times_, // times
   nsGkAtoms::transpose_, // transpose
   nsGkAtoms::_true, // true
   nsGkAtoms::union_, // union
   nsGkAtoms::uplimit_, // uplimit
   nsGkAtoms::variance_, // variance
   nsGkAtoms::vector_, // vector
   nsGkAtoms::vectorproduct_, // vectorproduct
   nsGkAtoms::xor_, // xor
  nullptr
};

const nsStaticAtom* const kAttributesMathML[] = {
   nsGkAtoms::accent_, // accent
   nsGkAtoms::accentunder_, // accentunder
   nsGkAtoms::actiontype_, // actiontype
   nsGkAtoms::align, // align
   nsGkAtoms::alignmentscope_, // alignmentscope
   nsGkAtoms::alt, // alt
   nsGkAtoms::altimg_, // altimg
   nsGkAtoms::altimg_height_, // altimg-height
   nsGkAtoms::altimg_valign_, // altimg-valign
   nsGkAtoms::altimg_width_, // altimg-width
   nsGkAtoms::background, // background
   nsGkAtoms::base, // base
   nsGkAtoms::bevelled_, // bevelled
   nsGkAtoms::cd_, // cd
   nsGkAtoms::cdgroup_, // cdgroup
   nsGkAtoms::charalign_, // charalign
   nsGkAtoms::close, // close
   nsGkAtoms::closure_, // closure
   nsGkAtoms::color, // color
   nsGkAtoms::columnalign_, // columnalign
   nsGkAtoms::columnalignment_, // columnalignment
   nsGkAtoms::columnlines_, // columnlines
   nsGkAtoms::columnspacing_, // columnspacing
   nsGkAtoms::columnspan_, // columnspan
   nsGkAtoms::columnwidth_, // columnwidth
   nsGkAtoms::crossout_, // crossout
   nsGkAtoms::decimalpoint_, // decimalpoint
   nsGkAtoms::definitionURL_, // definitionURL
   nsGkAtoms::denomalign_, // denomalign
   nsGkAtoms::depth_, // depth
   nsGkAtoms::dir, // dir
   nsGkAtoms::display, // display
   nsGkAtoms::displaystyle_, // displaystyle
   nsGkAtoms::edge_, // edge
   nsGkAtoms::encoding, // encoding
   nsGkAtoms::equalcolumns_, // equalcolumns
   nsGkAtoms::equalrows_, // equalrows
   nsGkAtoms::fence_, // fence
   nsGkAtoms::fontfamily_, // fontfamily
   nsGkAtoms::fontsize_, // fontsize
   nsGkAtoms::fontstyle_, // fontstyle
   nsGkAtoms::fontweight_, // fontweight
   nsGkAtoms::form, // form
   nsGkAtoms::frame, // frame
   nsGkAtoms::framespacing_, // framespacing
   nsGkAtoms::groupalign_, // groupalign
   nsGkAtoms::height, // height
   nsGkAtoms::href, // href
   nsGkAtoms::id, // id
   nsGkAtoms::indentalign_, // indentalign
   nsGkAtoms::indentalignfirst_, // indentalignfirst
   nsGkAtoms::indentalignlast_, // indentalignlast
   nsGkAtoms::indentshift_, // indentshift
   nsGkAtoms::indentshiftfirst_, // indentshiftfirst
   nsGkAtoms::indenttarget_, // indenttarget
   nsGkAtoms::index, // index
   nsGkAtoms::integer, // integer
   nsGkAtoms::largeop_, // largeop
   nsGkAtoms::length, // length
   nsGkAtoms::linebreak_, // linebreak
   nsGkAtoms::linebreakmultchar_, // linebreakmultchar
   nsGkAtoms::linebreakstyle_, // linebreakstyle
   nsGkAtoms::linethickness_, // linethickness
   nsGkAtoms::location_, // location
   nsGkAtoms::longdivstyle_, // longdivstyle
   nsGkAtoms::lquote_, // lquote
   nsGkAtoms::lspace_, // lspace
   nsGkAtoms::ltr, // ltr
   nsGkAtoms::mathbackground_, // mathbackground
   nsGkAtoms::mathcolor_, // mathcolor
   nsGkAtoms::mathsize_, // mathsize
   nsGkAtoms::mathvariant_, // mathvariant
   nsGkAtoms::maxsize_, // maxsize
   nsGkAtoms::minlabelspacing_, // minlabelspacing
   nsGkAtoms::minsize_, // minsize
   nsGkAtoms::movablelimits_, // movablelimits
   nsGkAtoms::msgroup_, // msgroup
   nsGkAtoms::name, // name
   nsGkAtoms::newline, // newline
   nsGkAtoms::notation_, // notation
   nsGkAtoms::numalign_, // numalign
   nsGkAtoms::number, // number
   nsGkAtoms::open, // open
   nsGkAtoms::order, // order
   nsGkAtoms::other, // other
   nsGkAtoms::overflow, // overflow
   nsGkAtoms::position, // position
   nsGkAtoms::role, // role
   nsGkAtoms::rowalign_, // rowalign
   nsGkAtoms::rowlines_, // rowlines
   nsGkAtoms::rowspacing_, // rowspacing
   nsGkAtoms::rowspan, // rowspan
   nsGkAtoms::rquote_, // rquote
   nsGkAtoms::rspace_, // rspace
   nsGkAtoms::schemaLocation_, // schemaLocation
   nsGkAtoms::scriptlevel_, // scriptlevel
   nsGkAtoms::scriptminsize_, // scriptminsize
   nsGkAtoms::scriptsize_, // scriptsize
   nsGkAtoms::scriptsizemultiplier_, // scriptsizemultiplier
   nsGkAtoms::selection_, // selection
   nsGkAtoms::separator_, // separator
   nsGkAtoms::separators_, // separators
   nsGkAtoms::shift_, // shift
   nsGkAtoms::side_, // side
   nsGkAtoms::src, // src
   nsGkAtoms::stackalign_, // stackalign
   nsGkAtoms::stretchy_, // stretchy
   nsGkAtoms::subscriptshift_, // subscriptshift
   nsGkAtoms::superscriptshift_, // superscriptshift
   nsGkAtoms::symmetric_, // symmetric
   nsGkAtoms::type, // type
   nsGkAtoms::voffset_, // voffset
   nsGkAtoms::width, // width
   nsGkAtoms::xref_, // xref
  nullptr
};

const nsStaticAtom* const kURLAttributesMathML[] = {
  nsGkAtoms::href,
  nsGkAtoms::src,
  nsGkAtoms::cdgroup_,
  nsGkAtoms::altimg_,
  nsGkAtoms::definitionURL_,
  nullptr
};

nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sElementsHTML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sAttributesHTML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sPresAttributesHTML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sElementsSVG = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sAttributesSVG = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sElementsMathML = nullptr;
nsTreeSanitizer::AtomsTable* nsTreeSanitizer::sAttributesMathML = nullptr;
nsIPrincipal* nsTreeSanitizer::sNullPrincipal = nullptr;

nsTreeSanitizer::nsTreeSanitizer(uint32_t aFlags)
 : mAllowStyles(aFlags & nsIParserUtils::SanitizerAllowStyle)
 , mAllowComments(aFlags & nsIParserUtils::SanitizerAllowComments)
 , mDropNonCSSPresentation(aFlags &
     nsIParserUtils::SanitizerDropNonCSSPresentation)
 , mDropForms(aFlags & nsIParserUtils::SanitizerDropForms)
 , mCidEmbedsOnly(aFlags &
     nsIParserUtils::SanitizerCidEmbedsOnly)
 , mDropMedia(aFlags & nsIParserUtils::SanitizerDropMedia)
 , mFullDocument(false)
 , mLogRemovals(aFlags & nsIParserUtils::SanitizerLogRemovals)
{
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

bool
nsTreeSanitizer::MustFlatten(int32_t aNamespace, nsAtom* aLocal)
{
  if (aNamespace == kNameSpaceID_XHTML) {
    if (mDropNonCSSPresentation && (nsGkAtoms::font == aLocal ||
                                    nsGkAtoms::center == aLocal)) {
      return true;
    }
    if (mDropForms && (nsGkAtoms::form == aLocal ||
                       nsGkAtoms::input == aLocal ||
                       nsGkAtoms::keygen == aLocal ||
                       nsGkAtoms::option == aLocal ||
                       nsGkAtoms::optgroup == aLocal)) {
      return true;
    }
    if (mFullDocument && (nsGkAtoms::title == aLocal ||
                          nsGkAtoms::html == aLocal ||
                          nsGkAtoms::head == aLocal ||
                          nsGkAtoms::body == aLocal)) {
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

bool
nsTreeSanitizer::IsURL(const nsStaticAtom* const* aURLs, nsAtom* aLocalName)
{
  const nsStaticAtom* atom;
  while ((atom = *aURLs)) {
    if (atom == aLocalName) {
      return true;
    }
    ++aURLs;
  }
  return false;
}

bool
nsTreeSanitizer::MustPrune(int32_t aNamespace,
                           nsAtom* aLocal,
                           mozilla::dom::Element* aElement)
{
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
    if (mDropForms && (nsGkAtoms::select == aLocal ||
                       nsGkAtoms::button == aLocal ||
                       nsGkAtoms::datalist == aLocal)) {
      return true;
    }
    if (mDropMedia && (nsGkAtoms::img == aLocal ||
                       nsGkAtoms::video == aLocal ||
                       nsGkAtoms::audio == aLocal ||
                       nsGkAtoms::source == aLocal)) {
      return true;
    }
    if (nsGkAtoms::meta == aLocal &&
        (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::charset) ||
         aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv))) {
      // Throw away charset declarations even if they also have microdata
      // which they can't validly have.
      return true;
    }
    if (((!mFullDocument && nsGkAtoms::meta == aLocal) ||
        nsGkAtoms::link == aLocal) &&
        !(aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop) ||
          aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::itemscope))) {
      // emulate old behavior for non-Microdata <meta> and <link> presumably
      // in <head>. <meta> and <link> are whitelisted in order to avoid
      // corrupting Microdata when they appear in <body>. Note that
      // SanitizeAttributes() will remove the rel attribute from <link> and
      // the name attribute from <meta>.
      return true;
    }
  }
  if (mAllowStyles) {
    if (nsGkAtoms::style == aLocal && !(aNamespace == kNameSpaceID_XHTML
        || aNamespace == kNameSpaceID_SVG)) {
      return true;
    }
    return false;
  }
  if (nsGkAtoms::style == aLocal) {
    return true;
  }
  return false;
}

bool
nsTreeSanitizer::SanitizeStyleDeclaration(DeclarationBlock* aDeclaration)
{
  return aDeclaration->RemovePropertyByID(eCSSProperty__moz_binding);
}

bool
nsTreeSanitizer::SanitizeStyleSheet(const nsAString& aOriginal,
                                    nsAString& aSanitized,
                                    nsIDocument* aDocument,
                                    nsIURI* aBaseURI)
{
  nsresult rv = NS_OK;
  aSanitized.Truncate();
  // aSanitized will hold the permitted CSS text.
  // -moz-binding is blacklisted.
  bool didSanitize = false;
  // Create a sheet to hold the parsed CSS
  RefPtr<StyleSheet> sheet =
    new StyleSheet(mozilla::css::eAuthorSheetFeatures,
                   CORS_NONE,
                   aDocument->GetReferrerPolicy(),
                   SRIMetadata());
  sheet->SetURIs(aDocument->GetDocumentURI(), nullptr, aBaseURI);
  sheet->SetPrincipal(aDocument->NodePrincipal());
  sheet->ParseSheetSync(
    aDocument->CSSLoader(),
    NS_ConvertUTF16toUTF8(aOriginal),
    /* aLoadData = */ nullptr,
    /* aLineNumber = */ 0);
  NS_ENSURE_SUCCESS(rv, true);
  // Mark the sheet as complete.
  MOZ_ASSERT(!sheet->HasForcedUniqueInner(),
             "should not get a forced unique inner during parsing");
  sheet->SetComplete();
  // Loop through all the rules found in the CSS text
  ErrorResult err;
  RefPtr<dom::CSSRuleList> rules =
    sheet->GetCssRules(*nsContentUtils::GetSystemPrincipal(), err);
  err.SuppressException();
  if (!rules) {
    return true;
  }
  uint32_t ruleCount = rules->Length();
  for (uint32_t i = 0; i < ruleCount; ++i) {
    mozilla::css::Rule* rule = rules->Item(i);
    if (!rule)
      continue;
    switch (rule->Type()) {
      default:
        didSanitize = true;
        // Ignore these rule types.
        break;
      case CSSRule_Binding::NAMESPACE_RULE:
      case CSSRule_Binding::FONT_FACE_RULE: {
        // Append @namespace and @font-face rules verbatim.
        nsAutoString cssText;
        rule->GetCssText(cssText);
        aSanitized.Append(cssText);
        break;
      }
      case CSSRule_Binding::STYLE_RULE: {
        // For style rules, we will just look for and remove the
        // -moz-binding properties.
        auto styleRule = static_cast<BindingStyleRule*>(rule);
        DeclarationBlock* styleDecl = styleRule->GetDeclarationBlock();
        MOZ_ASSERT(styleDecl);
        if (SanitizeStyleDeclaration(styleDecl)) {
          didSanitize = true;
        }
        nsAutoString decl;
        styleRule->GetCssText(decl);
        aSanitized.Append(decl);
      }
    }
  }
  if (didSanitize && mLogRemovals) {
    LogMessage("Removed some rules and/or properties from stylesheet.", aDocument);
  }
  return didSanitize;
}

template<size_t Len>
static bool
UTF16StringStartsWith(const char16_t* aStr, uint32_t aLength,
                      const char16_t (&aNeedle)[Len])
{
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

void
nsTreeSanitizer::SanitizeAttributes(mozilla::dom::Element* aElement,
                                    AllowedAttributes aAllowed)
{
  uint32_t ac = aElement->GetAttrCount();

  for (int32_t i = ac - 1; i >= 0; --i) {
    const nsAttrName* attrName = aElement->GetAttrNameAt(i);
    int32_t attrNs = attrName->NamespaceID();
    RefPtr<nsAtom> attrLocal = attrName->LocalName();

    if (kNameSpaceID_None == attrNs) {
      if (aAllowed.mStyle && nsGkAtoms::style == attrLocal) {
        nsAutoString value;
        aElement->GetAttr(attrNs, attrLocal, value);
        nsIDocument* document = aElement->OwnerDoc();
        RefPtr<URLExtraData> urlExtra(aElement->GetURLDataForStyleAttr());
        RefPtr<DeclarationBlock> decl =
          DeclarationBlock::FromCssText(
            value,
            urlExtra,
            document->GetCompatibilityMode(),
            document->CSSLoader());
        if (decl) {
          if (SanitizeStyleDeclaration(decl)) {
            nsAutoString cleanValue;
            decl->ToString(cleanValue);
            aElement->SetAttr(kNameSpaceID_None,
                              nsGkAtoms::style,
                              cleanValue,
                              false);
            if (mLogRemovals) {
              LogMessage("Removed -moz-binding styling from element style attribute.",
                         aElement->OwnerDoc(), aElement);
            }
          }
        }
        continue;
      }
      if (aAllowed.mDangerousSrc && nsGkAtoms::src == attrLocal) {
        continue;
      }
      if (IsURL(aAllowed.mURLs, attrLocal)) {
        if (SanitizeURL(aElement, attrNs, attrLocal)) {
          // in case the attribute removal shuffled the attribute order, start
          // the loop again.
          --ac;
          i = ac; // i will be decremented immediately thanks to the for loop
          continue;
        }
        // else fall through to see if there's another reason to drop this
        // attribute (in particular if the attribute is background="" on an
        // HTML element)
      }
      if (!mDropNonCSSPresentation &&
          (aAllowed.mNames == sAttributesHTML) && // element is HTML
          sPresAttributesHTML->Contains(attrLocal)) {
        continue;
      }
      if (aAllowed.mNames->Contains(attrLocal) &&
          !((attrLocal == nsGkAtoms::rel &&
             aElement->IsHTMLElement(nsGkAtoms::link)) ||
            (!mFullDocument &&
             attrLocal == nsGkAtoms::name &&
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
      if (nsGkAtoms::base == attrLocal) {
        if (SanitizeURL(aElement, attrNs, attrLocal)) {
          // in case the attribute removal shuffled the attribute order, start
          // the loop again.
          --ac;
          i = ac; // i will be decremented immediately thanks to the for loop
        }
        continue;
      }
      if (nsGkAtoms::lang == attrLocal || nsGkAtoms::space == attrLocal) {
        continue;
      }
      // else not allowed
    } else if (aAllowed.mXLink && kNameSpaceID_XLink == attrNs) {
      if (nsGkAtoms::href == attrLocal) {
        if (SanitizeURL(aElement, attrNs, attrLocal)) {
          // in case the attribute removal shuffled the attribute order, start
          // the loop again.
          --ac;
          i = ac; // i will be decremented immediately thanks to the for loop
        }
        continue;
      }
      if (nsGkAtoms::type == attrLocal || nsGkAtoms::title == attrLocal
          || nsGkAtoms::show == attrLocal || nsGkAtoms::actuate == attrLocal) {
        continue;
      }
      // else not allowed
    }
    aElement->UnsetAttr(kNameSpaceID_None, attrLocal, false);
    if (mLogRemovals) {
      LogMessage("Removed unsafe attribute.", aElement->OwnerDoc(),
                 aElement, attrLocal);
    }
    // in case the attribute removal shuffled the attribute order, start the
    // loop again.
    --ac;
    i = ac; // i will be decremented immediately thanks to the for loop
  }

  // If we've got HTML audio or video, add the controls attribute, because
  // otherwise the content is unplayable with scripts removed.
  if (aElement->IsAnyOfHTMLElements(nsGkAtoms::video, nsGkAtoms::audio)) {
    aElement->SetAttr(kNameSpaceID_None,
                      nsGkAtoms::controls,
                      EmptyString(),
                      false);
  }
}

bool
nsTreeSanitizer::SanitizeURL(mozilla::dom::Element* aElement,
                             int32_t aNamespace,
                             nsAtom* aLocalName)
{
  nsAutoString value;
  aElement->GetAttr(aNamespace, aLocalName, value);

  // Get value and remove mandatory quotes
  static const char* kWhitespace = "\n\r\t\b";
  const nsAString& v =
    nsContentUtils::TrimCharsInSet(kWhitespace, value);
  // Fragment-only url cannot be harmful.
  if (!v.IsEmpty() && v.First() == u'#') {
    return false;
  }

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  uint32_t flags = nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL;

  nsCOMPtr<nsIURI> baseURI = aElement->GetBaseURI();
  nsCOMPtr<nsIURI> attrURI;
  nsresult rv = NS_NewURI(getter_AddRefs(attrURI), v, nullptr, baseURI);
  if (NS_SUCCEEDED(rv)) {
    if (mCidEmbedsOnly &&
        kNameSpaceID_None == aNamespace) {
      if (nsGkAtoms::src == aLocalName || nsGkAtoms::background == aLocalName) {
        // comm-central uses a hack that makes nsIURIs created with cid: specs
        // actually have an about:blank spec. Therefore, nsIURI facilities are
        // useless for cid: when comm-central code is participating.
        if (!(v.Length() > 4 &&
              (v[0] == 'c' || v[0] == 'C') &&
              (v[1] == 'i' || v[1] == 'I') &&
              (v[2] == 'd' || v[2] == 'D') &&
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
        rv = secMan->CheckLoadURIWithPrincipal(sNullPrincipal, attrURI, flags);
      }
    } else {
      rv = secMan->CheckLoadURIWithPrincipal(sNullPrincipal, attrURI, flags);
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

void
nsTreeSanitizer::Sanitize(DocumentFragment* aFragment)
{
  // If you want to relax these preconditions, be sure to check the code in
  // here that notifies / does not notify or that fires mutation events if
  // in tree.
  MOZ_ASSERT(!aFragment->IsInUncomposedDoc(), "The fragment is in doc?");

  mFullDocument = false;
  SanitizeChildren(aFragment);
}

void
nsTreeSanitizer::Sanitize(nsIDocument* aDocument)
{
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

void
nsTreeSanitizer::SanitizeChildren(nsINode* aRoot)
{
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
      if (nsGkAtoms::style == localName) {
        // If styles aren't allowed, style elements got pruned above. Even
        // if styles are allowed, non-HTML, non-SVG style elements got pruned
        // above.
        NS_ASSERTION(ns == kNameSpaceID_XHTML || ns == kNameSpaceID_SVG,
            "Should have only HTML or SVG here!");
        nsAutoString styleText;
        nsContentUtils::GetNodeTextContent(node, false, styleText);

        nsAutoString sanitizedStyle;
        nsCOMPtr<nsIURI> baseURI = node->GetBaseURI();
        if (SanitizeStyleSheet(styleText,
                               sanitizedStyle,
                               aRoot->OwnerDoc(),
                               baseURI)) {
          nsContentUtils::SetNodeTextContent(node, sanitizedStyle, true);
        } else {
          // If the node had non-text child nodes, this operation zaps those.
          //XXXgijs: if we're logging, we should theoretically report about
          // this, but this way of removing those items doesn't allow for that
          // to happen. Seems less likely to be a problem for actual chrome
          // consumers though.
          nsContentUtils::SetNodeTextContent(node, styleText, true);
        }
        AllowedAttributes allowed;
        allowed.mStyle = mAllowStyles;
        if (ns == kNameSpaceID_XHTML) {
          allowed.mNames = sAttributesHTML;
          allowed.mURLs = kURLAttributesHTML;
          SanitizeAttributes(elt, allowed);
        } else {
          allowed.mNames = sAttributesSVG;
          allowed.mURLs = kURLAttributesSVG;
          allowed.mXLink = true;
          SanitizeAttributes(elt, allowed);
        }
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
        nsCOMPtr<nsIContent> child; // Must keep the child alive during move
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
      NS_ASSERTION(ns == kNameSpaceID_XHTML ||
                   ns == kNameSpaceID_SVG ||
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

void
nsTreeSanitizer::RemoveAllAttributes(Element* aElement)
{
  const nsAttrName* attrName;
  while ((attrName = aElement->GetAttrNameAt(0))) {
    int32_t attrNs = attrName->NamespaceID();
    RefPtr<nsAtom> attrLocal = attrName->LocalName();
    aElement->UnsetAttr(attrNs, attrLocal, false);
  }
}

void
nsTreeSanitizer::LogMessage(const char* aMessage, nsIDocument* aDoc,
                            Element* aElement, nsAtom* aAttr)
{
  if (mLogRemovals) {
    nsAutoString msg;
    msg.Assign(NS_ConvertASCIItoUTF16(aMessage));
    if (aElement) {
      msg.Append(NS_LITERAL_STRING(" Element: ") + aElement->LocalName() +
                 NS_LITERAL_STRING("."));
    }
    if (aAttr) {
      msg.Append(NS_LITERAL_STRING(" Attribute: ") +
                 nsDependentAtomString(aAttr) + NS_LITERAL_STRING("."));
    }

    nsContentUtils::ReportToConsoleNonLocalized(
        msg, nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM"), aDoc);
  }
}

void
nsTreeSanitizer::InitializeStatics()
{
  MOZ_ASSERT(!sElementsHTML, "Initializing a second time.");

  sElementsHTML = new AtomsTable(ArrayLength(kElementsHTML));
  for (uint32_t i = 0; kElementsHTML[i]; i++) {
    sElementsHTML->PutEntry(kElementsHTML[i]);
  }

  sAttributesHTML = new AtomsTable(ArrayLength(kAttributesHTML));
  for (uint32_t i = 0; kAttributesHTML[i]; i++) {
    sAttributesHTML->PutEntry(kAttributesHTML[i]);
  }

  sPresAttributesHTML = new AtomsTable(ArrayLength(kPresAttributesHTML));
  for (uint32_t i = 0; kPresAttributesHTML[i]; i++) {
    sPresAttributesHTML->PutEntry(kPresAttributesHTML[i]);
  }

  sElementsSVG = new AtomsTable(ArrayLength(kElementsSVG));
  for (uint32_t i = 0; kElementsSVG[i]; i++) {
    sElementsSVG->PutEntry(kElementsSVG[i]);
  }

  sAttributesSVG = new AtomsTable(ArrayLength(kAttributesSVG));
  for (uint32_t i = 0; kAttributesSVG[i]; i++) {
    sAttributesSVG->PutEntry(kAttributesSVG[i]);
  }

  sElementsMathML = new AtomsTable(ArrayLength(kElementsMathML));
  for (uint32_t i = 0; kElementsMathML[i]; i++) {
    sElementsMathML->PutEntry(kElementsMathML[i]);
  }

  sAttributesMathML = new AtomsTable(ArrayLength(kAttributesMathML));
  for (uint32_t i = 0; kAttributesMathML[i]; i++) {
    sAttributesMathML->PutEntry(kAttributesMathML[i]);
  }

  nsCOMPtr<nsIPrincipal> principal = NullPrincipal::CreateWithoutOriginAttributes();
  principal.forget(&sNullPrincipal);
}

void
nsTreeSanitizer::ReleaseStatics()
{
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

  NS_IF_RELEASE(sNullPrincipal);
}
