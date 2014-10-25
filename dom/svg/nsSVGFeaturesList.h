/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//This file must not have include guards.

// Features (SVG 1.1 style)

// Static festures
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#CoreAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Structure")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ContainerAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ConditionalProcessing")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Image")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Style")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ViewportAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Shape")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Text")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#PaintAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#OpacityAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#GraphicsAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Marker")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ColorProfile")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Gradient")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Pattern")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Clip")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Mask")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Filter")

// Basic features
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicStructure")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicText")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicPaintAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicGraphicsAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicClip")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicFilter")

// Animation feature
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Animation")

// Dynamic features
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#DocumentEventsAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#GraphicalEventsAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#AnimationEventsAttribute")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Cursor")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Hyperlinking")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#XlinkAttribute")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ExternalResourcesRequired")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#View")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Font")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicFont")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Extensibility")

// Feature groups (SVG 1.1 style)
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG-static")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM-static")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG-animation")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM-animation")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG-dynamic")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM-dynamic")

// Features (SVG 1.0 style)
SVG_UNSUPPORTED_FEATURE("org.w3c.svg")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.all")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.static")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.animation")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.dynamic")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.all")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.static")
SVG_SUPPORTED_FEATURE("org.w3c.dom.svg.animation")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.dynamic")
