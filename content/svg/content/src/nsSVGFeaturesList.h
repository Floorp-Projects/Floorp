/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

// Features (SVG 1.1 style)

// Static festures
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#CoreAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Structure")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ContainerAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ConditionalProcessing")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Image")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Style")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ViewportAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Shape")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Text")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#PaintAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#OpacityAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#GraphicsAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Marker")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ColorProfile")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Gradient")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Pattern")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Clip")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Mask")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Filter")

// Basic features
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicStructure")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicText")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicPaintAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicGraphicsAttribute")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicClip")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicFilter")

// Animation feature
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Animation")

// Dynamic features
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#DocumentEventsAttribute")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#GraphicalEventsAttribute")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#AnimationEventsAttribute")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Cursor")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Hyperlinking")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#XlinkAttribute")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#ExternalResourcesRequired")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#View")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Script")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Font")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#BasicFont")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#Extensibility")

// Feature groups (SVG 1.1 style)
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG")
SVG_SUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG-static")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM-static")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG-animation")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM-animation")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVG-dynamic")
SVG_UNSUPPORTED_FEATURE("http://www.w3.org/TR/SVG11/feature#SVGDOM-dynamic")

// Features (SVG 1.0 style)
SVG_SUPPORTED_FEATURE("org.w3c.svg")
SVG_SUPPORTED_FEATURE("org.w3c.svg.lang")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.all")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.static")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.animation")
SVG_UNSUPPORTED_FEATURE("org.w3c.svg.dynamic")
SVG_SUPPORTED_FEATURE("org.w3c.dom.svg")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.all")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.static")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.animation")
SVG_UNSUPPORTED_FEATURE("org.w3c.dom.svg.dynamic")
