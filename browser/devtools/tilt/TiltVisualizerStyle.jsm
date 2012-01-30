/* -*- Mode: javascript, tab-width: 2, indent-tabs-mode: nil, c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"), you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Tilt: A WebGL-based 3D visualization of a webpage.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Victor Porof <vporof@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 ***** END LICENSE BLOCK *****/
"use strict";

const Cu = Components.utils;

Cu.import("resource:///modules/devtools/TiltMath.jsm");

let EXPORTED_SYMBOLS = ["TiltVisualizerStyle"];
let rgba = TiltMath.hex2rgba;

/**
 * Various colors and style settings used throughout Tilt.
 */
let TiltVisualizerStyle = {

  canvas: {
    background: "-moz-linear-gradient(top, #454545 0%, #000 100%)",
  },

  nodes: {
    highlight: {
      defaultFill: rgba("#555"),
      defaultStroke: rgba("#000"),
      defaultStrokeWeight: 1
    },

    html: rgba("#8880"),
    body: rgba("#fff0"),
    h1: rgba("#e667af"),
    h2: rgba("#c667af"),
    h3: rgba("#a667af"),
    h4: rgba("#8667af"),
    h5: rgba("#8647af"),
    h6: rgba("#8627af"),
    div: rgba("#5dc8cd"),
    span: rgba("#67e46f"),
    table: rgba("#ff0700"),
    tr: rgba("#ff4540"),
    td: rgba("#ff7673"),
    ul: rgba("#4671d5"),
    li: rgba("#6c8cd5"),
    p: rgba("#aaa"),
    a: rgba("#123eab"),
    img: rgba("#ffb473"),
    iframe: rgba("#85004b")
  }
};
