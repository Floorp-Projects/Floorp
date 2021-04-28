/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Created from build/server/static/css/inline-selection.css */
window.inlineSelectionCss = `
.button, .highlight-button-cancel, .highlight-button-download, .highlight-button-copy {
  display: flex;
  align-items: center;
  justify-content: center;
  border: 0;
  border-radius: 3px;
  cursor: pointer;
  font-size: 16px;
  font-weight: 400;
  height: 40px;
  min-width: 40px;
  outline: none;
  padding: 0 10px;
  position: relative;
  text-align: center;
  text-decoration: none;
  transition: background 150ms cubic-bezier(0.07, 0.95, 0, 1), border 150ms cubic-bezier(0.07, 0.95, 0, 1);
  user-select: none;
  white-space: nowrap; }
  .button.hidden, .hidden.highlight-button-cancel, .hidden.highlight-button-download, .hidden.highlight-button-copy {
    display: none; }
  .button.small, .small.highlight-button-cancel, .small.highlight-button-download, .small.highlight-button-copy {
    height: 32px;
    line-height: 32px;
    padding: 0 8px; }
  .button.active, .active.highlight-button-cancel, .active.highlight-button-download, .active.highlight-button-copy {
    background-color: #dedede; }
  .button.tiny, .tiny.highlight-button-cancel, .tiny.highlight-button-download, .tiny.highlight-button-copy {
    font-size: 14px;
    height: 26px;
    border: 1px solid #c7c7c7; }
    .button.tiny:hover, .tiny.highlight-button-cancel:hover, .tiny.highlight-button-download:hover, .tiny.highlight-button-copy:hover, .button.tiny:focus, .tiny.highlight-button-cancel:focus, .tiny.highlight-button-download:focus, .tiny.highlight-button-copy:focus {
      background: #ededf0;
      border-color: #989898; }
    .button.tiny:active, .tiny.highlight-button-cancel:active, .tiny.highlight-button-download:active, .tiny.highlight-button-copy:active {
      background: #dedede;
      border-color: #989898; }
  .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-download, .block-button.highlight-button-copy {
    display: flex;
    align-items: center;
    justify-content: center;
    box-sizing: border-box;
    border: 0;
    border-inline-end: 1px solid #c7c7c7;
    box-shadow: 0;
    border-radius: 0;
    flex-shrink: 0;
    font-size: 20px;
    height: 100px;
    line-height: 100%;
    overflow: hidden; }
    @media (max-width: 719px) {
      .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-download, .block-button.highlight-button-copy {
        justify-content: flex-start;
        font-size: 16px;
        height: 72px;
        margin-inline-end: 10px;
        padding: 0 5px; } }
    .button.block-button:hover, .block-button.highlight-button-cancel:hover, .block-button.highlight-button-download:hover, .block-button.highlight-button-copy:hover {
      background: #ededf0; }
    .button.block-button:active, .block-button.highlight-button-cancel:active, .block-button.highlight-button-download:active, .block-button.highlight-button-copy:active {
      background: #dedede; }
  .button.download, .download.highlight-button-cancel, .download.highlight-button-download, .download.highlight-button-copy, .button.edit, .edit.highlight-button-cancel, .edit.highlight-button-download, .edit.highlight-button-copy, .button.trash, .trash.highlight-button-cancel, .trash.highlight-button-download, .trash.highlight-button-copy, .button.share, .share.highlight-button-cancel, .share.highlight-button-download, .share.highlight-button-copy, .button.flag, .flag.highlight-button-cancel, .flag.highlight-button-download, .flag.highlight-button-copy {
    background-repeat: no-repeat;
    background-size: 50%;
    background-position: center;
    margin-inline-end: 10px;
    transition: background-color 150ms cubic-bezier(0.07, 0.95, 0, 1); }
  .button.download, .download.highlight-button-cancel, .download.highlight-button-download, .download.highlight-button-copy {
    background-image: url("../img/icon-download.svg"); }
    .button.download:hover, .download.highlight-button-cancel:hover, .download.highlight-button-download:hover, .download.highlight-button-copy:hover {
      background-color: #ededf0; }
    .button.download:active, .download.highlight-button-cancel:active, .download.highlight-button-download:active, .download.highlight-button-copy:active {
      background-color: #dedede; }
  .button.share, .share.highlight-button-cancel, .share.highlight-button-download, .share.highlight-button-copy {
    background-image: url("../img/icon-share.svg"); }
    .button.share:hover, .share.highlight-button-cancel:hover, .share.highlight-button-download:hover, .share.highlight-button-copy:hover {
      background-color: #ededf0; }
    .button.share.active, .share.active.highlight-button-cancel, .share.active.highlight-button-download, .share.active.highlight-button-copy, .button.share:active, .share.highlight-button-cancel:active, .share.highlight-button-download:active, .share.highlight-button-copy:active {
      background-color: #dedede; }
  .button.share.newicon, .share.newicon.highlight-button-cancel, .share.newicon.highlight-button-download, .share.newicon.highlight-button-copy {
    background-image: url("../img/icon-share-alternate.svg"); }
  .button.trash, .trash.highlight-button-cancel, .trash.highlight-button-download, .trash.highlight-button-copy {
    background-image: url("../img/icon-trash.svg"); }
    .button.trash:hover, .trash.highlight-button-cancel:hover, .trash.highlight-button-download:hover, .trash.highlight-button-copy:hover {
      background-color: #ededf0; }
    .button.trash:active, .trash.highlight-button-cancel:active, .trash.highlight-button-download:active, .trash.highlight-button-copy:active {
      background-color: #dedede; }
  .button.edit, .edit.highlight-button-cancel, .edit.highlight-button-download, .edit.highlight-button-copy {
    background-image: url("../img/icon-edit.svg"); }
    .button.edit:hover, .edit.highlight-button-cancel:hover, .edit.highlight-button-download:hover, .edit.highlight-button-copy:hover {
      background-color: #ededf0; }
    .button.edit:active, .edit.highlight-button-cancel:active, .edit.highlight-button-download:active, .edit.highlight-button-copy:active {
      background-color: #dedede; }

.app-body {
  background: #f9f9fa;
  color: #38383d; }
  .app-body a {
    color: #0a84ff; }

.highlight-color-scheme {
  background: #0a84ff;
  color: #fff; }
  .highlight-color-scheme a {
    color: #fff;
    text-decoration: underline; }

.alt-color-scheme {
  background: #38383d;
  color: #f9f9fa; }
  .alt-color-scheme h1 {
    color: #6f7fb6; }
  .alt-color-scheme a {
    color: #e1e1e6;
    text-decoration: underline; }

.button.primary, .primary.highlight-button-cancel, .highlight-button-download, .primary.highlight-button-copy {
  background-color: #0a84ff;
  color: #fff; }
  .button.primary:hover, .primary.highlight-button-cancel:hover, .highlight-button-download:hover, .primary.highlight-button-copy:hover, .button.primary:focus, .primary.highlight-button-cancel:focus, .highlight-button-download:focus, .primary.highlight-button-copy:focus {
    background-color: #0072e5; }
  .button.primary:active, .primary.highlight-button-cancel:active, .highlight-button-download:active, .primary.highlight-button-copy:active {
    background-color: #0065cc; }

.button.secondary, .highlight-button-cancel, .secondary.highlight-button-download, .highlight-button-copy {
  background-color: #f9f9fa;
  color: #38383d; }
  .button.secondary:hover, .highlight-button-cancel:hover, .secondary.highlight-button-download:hover, .highlight-button-copy:hover {
    background-color: #ededf0; }
  .button.secondary:active, .highlight-button-cancel:active, .secondary.highlight-button-download:active, .highlight-button-copy:active {
    background-color: #dedede; }

.button.transparent, .transparent.highlight-button-cancel, .transparent.highlight-button-download, .transparent.highlight-button-copy {
  background-color: transparent;
  color: #38383d; }
  .button.transparent:hover, .transparent.highlight-button-cancel:hover, .transparent.highlight-button-download:hover, .transparent.highlight-button-copy:hover {
    background-color: #ededf0; }
  .button.transparent:focus, .transparent.highlight-button-cancel:focus, .transparent.highlight-button-download:focus, .transparent.highlight-button-copy:focus, .button.transparent:active, .transparent.highlight-button-cancel:active, .transparent.highlight-button-download:active, .transparent.highlight-button-copy:active {
    background-color: #dedede; }

.button.warning, .warning.highlight-button-cancel, .warning.highlight-button-download, .warning.highlight-button-copy {
  color: #fff;
  background: #d92215; }
  .button.warning:hover, .warning.highlight-button-cancel:hover, .warning.highlight-button-download:hover, .warning.highlight-button-copy:hover, .button.warning:focus, .warning.highlight-button-cancel:focus, .warning.highlight-button-download:focus, .warning.highlight-button-copy:focus {
    background: #b81d12; }
  .button.warning:active, .warning.highlight-button-cancel:active, .warning.highlight-button-download:active, .warning.highlight-button-copy:active {
    background: #a11910; }

.subtitle-link {
  color: #0a84ff; }

.loader {
  background: rgba(12, 12, 13, 0.2);
  border-radius: 2px;
  height: 4px;
  overflow: hidden;
  position: relative;
  width: 200px; }

.loader-inner {
  animation: bounce infinite alternate 1250ms cubic-bezier(0.7, 0, 0.3, 1);
  background: #45a1ff;
  border-radius: 2px;
  height: 4px;
  transform: translateX(-40px);
  width: 50px; }

@keyframes bounce {
  0% {
    transform: translateX(-40px); }
  100% {
    transform: translate(190px); } }

@keyframes fade-in {
  0% {
    opacity: 0; }
  100% {
    opacity: 1; } }

@keyframes pop {
  0% {
    transform: scale(1); }
  97% {
    transform: scale(1.04); }
  100% {
    transform: scale(1); } }

@keyframes pulse {
  0% {
    opacity: 0.3;
    transform: scale(1); }
  70% {
    opacity: 0.25;
    transform: scale(1.04); }
  100% {
    opacity: 0.3;
    transform: scale(1); } }

@keyframes slide-left {
  0% {
    opacity: 0;
    transform: translate3d(160px, 0, 0); }
  100% {
    opacity: 1;
    transform: translate3d(0, 0, 0); } }

@keyframes bounce-in {
  0% {
    opacity: 0;
    transform: scale(1); }
  60% {
    opacity: 1;
    transform: scale(1.02); }
  100% {
    transform: scale(1); } }

.mover-target {
  display: flex;
  align-items: center;
  justify-content: center;
  pointer-events: auto;
  position: absolute;
  z-index: 5; }

.highlight,
.mover-target {
  background-color: transparent;
  background-image: none; }

.mover-target,
.bghighlight {
  border: 0; }

.hover-highlight {
  animation: fade-in 125ms forwards cubic-bezier(0.07, 0.95, 0, 1);
  background: rgba(255, 255, 255, 0.2);
  border-radius: 1px;
  pointer-events: none;
  position: absolute;
  z-index: 10000000000; }
  .hover-highlight::before {
    border: 2px dashed rgba(255, 255, 255, 0.4);
    bottom: 0;
    content: "";
    inset-inline-start: 0;
    position: absolute;
    inset-inline-end: 0;
    top: 0; }
    /* When prefers contrast is fully supported, we should change these quereies to cover both high and low prefers contrast cases */
    @media (forced-colors: active) {
      .hover-highlight {
        background-color: white;
        opacity: 0.2; } }

.mover-target.direction-topLeft {
  cursor: nwse-resize;
  height: 60px;
  left: -30px;
  top: -30px;
  width: 60px; }

.mover-target.direction-top {
  cursor: ns-resize;
  height: 60px;
  inset-inline-start: 0;
  top: -30px;
  width: 100%;
  z-index: 4; }

.mover-target.direction-topRight {
  cursor: nesw-resize;
  height: 60px;
  right: -30px;
  top: -30px;
  width: 60px; }

.mover-target.direction-left {
  cursor: ew-resize;
  height: 100%;
  left: -30px;
  top: 0;
  width: 60px;
  z-index: 4; }

.mover-target.direction-right {
  cursor: ew-resize;
  height: 100%;
  right: -30px;
  top: 0;
  width: 60px;
  z-index: 4; }

.mover-target.direction-bottomLeft {
  bottom: -30px;
  cursor: nesw-resize;
  height: 60px;
  left: -30px;
  width: 60px; }

.mover-target.direction-bottom {
  bottom: -30px;
  cursor: ns-resize;
  height: 60px;
  inset-inline-start: 0;
  width: 100%;
  z-index: 4; }

.mover-target.direction-bottomRight {
  bottom: -30px;
  cursor: nwse-resize;
  height: 60px;
  right: -30px;
  width: 60px; }

.mover-target:hover .mover {
  transform: scale(1.05); }

.mover {
  background-color: #fff;
  border-radius: 50%;
  box-shadow: 0 0 4px rgba(0, 0, 0, 0.5);
  height: 16px;
  opacity: 1;
  position: relative;
  transition: transform 125ms cubic-bezier(0.07, 0.95, 0, 1);
  width: 16px; }
  .small-selection .mover {
    height: 10px;
    width: 10px; }

.direction-topLeft .mover,
.direction-left .mover,
.direction-bottomLeft .mover {
  left: -1px; }

.direction-topLeft .mover,
.direction-top .mover,
.direction-topRight .mover {
  top: -1px; }

.direction-topRight .mover,
.direction-right .mover,
.direction-bottomRight .mover {
  right: -1px; }

.direction-bottomRight .mover,
.direction-bottom .mover,
.direction-bottomLeft .mover {
  bottom: -1px; }

.bghighlight {
  background-color: rgba(0, 0, 0, 0.7);
  position: absolute;
  z-index: 9999999999; }
  /* When prefers contrast is fully supported, we should change these quereies to cover both high and low prefers contrast cases */
  @media (forced-colors: active) {
    .bghighlight {
      background-color: black;
      opacity: 0.7; } }

.preview-overlay {
  align-items: center;
  background-color: rgba(0, 0, 0, 0.7);
  display: flex;
  height: 100%;
  justify-content: center;
  inset-inline-start: 0;
  margin: 0;
  padding: 0;
  position: fixed;
  top: 0;
  width: 100%;
  z-index: 9999999999; }
  /* When prefers contrast is fully supported, we should change these quereies to cover both high and low prefers contrast cases */
  @media (forced-colors: active) {
    .preview-overlay {
      background-color: black;
      opacity: 0.7; } }

.precision-cursor {
  cursor: crosshair; }

.highlight {
  border-radius: 1px;
  border: 2px dashed rgba(255, 255, 255, 0.8);
  box-sizing: border-box;
  cursor: move;
  position: absolute;
  z-index: 9999999999; }
  /* When prefers contrast is fully supported, we should change these quereies to cover both high and low prefers contrast cases */
  @media (forced-colors: active) {
    .highlight {
      border: 2px dashed white;
      opacity: 1.0; } }

.highlight-buttons {
  display: flex;
  align-items: center;
  justify-content: center;
  bottom: -58px;
  position: absolute;
  inset-inline-end: 5px;
  z-index: 6; }
  .bottom-selection .highlight-buttons {
    bottom: 5px; }
  .left-selection .highlight-buttons {
    inset-inline-end: auto;
    inset-inline-start: 5px; }
  .highlight-buttons > button {
    box-shadow: 0 0 0 1px rgba(12, 12, 13, 0.1), 0 2px 8px rgba(12, 12, 13, 0.1); }

.highlight-button-cancel {
  margin: 5px;
  width: 40px; }

.highlight-button-download {
  margin: 5px;
  width: auto;
  font-size: 18px; }

.highlight-button-download img {
    height: 16px;
    width: 16px;
}

.highlight-button-download:-moz-locale-dir(rtl) {
  flex-direction: reverse;
}

.highlight-button-download img:-moz-locale-dir(ltr) {
  padding-inline-end: 8px;
}

.highlight-button-download img:-moz-locale-dir(rtl) {
  padding-inline-start: 8px;
}

.highlight-button-copy {
  margin: 5px;
  width: auto; }

.highlight-button-copy img {
    height: 16px;
    width: 16px;
}

.highlight-button-copy:-moz-locale-dir(rtl) {
  flex-direction: reverse;
}

.highlight-button-copy img:-moz-locale-dir(ltr) {
  padding-inline-end: 8px;
}

.highlight-button-copy img:-moz-locale-dir(rtl) {
  padding-inline-start: 8px;
}

.pixel-dimensions {
  position: absolute;
  pointer-events: none;
  font-weight: bold;
  font-family: -apple-system, BlinkMacSystemFont, "segoe ui", "helvetica neue", helvetica, ubuntu, roboto, noto, arial, sans-serif;
  font-size: 70%;
  color: #000;
  text-shadow: -1px -1px 0 #fff, 1px -1px 0 #fff, -1px 1px 0 #fff, 1px 1px 0 #fff; }

.preview-buttons {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  padding-inline-end: 4px;
  inset-inline-end: 0;
  width: 100%;
  position: absolute;
  height: 60px;
  border-radius: 4px 4px 0 0;
  background: rgba(249, 249, 250, 0.8);
  top: 0;
  border: 1px solid rgba(249, 249, 250, 0.2);
  border-bottom: 0;
  box-sizing: border-box; }

.preview-image {
  display: flex;
  align-items: center;
  flex-direction: column;
  justify-content: center;
  margin: 24px auto;
  position: relative;
  max-width: 80%;
  max-height: 95%;
  text-align: center;
  animation-delay: 50ms;
  display: flex; }

.preview-image-wrapper {
  background: rgba(249, 249, 250, 0.8);
  border-radius: 0 0 4px 4px;
  display: block;
  height: auto;
  max-width: 100%;
  min-width: 320px;
  overflow-y: scroll;
  padding: 0 60px;
  margin-top: 60px;
  border: 1px solid rgba(249, 249, 250, 0.2);
  border-top: 0; }

.preview-image-wrapper > img {
  box-shadow: 0 0 0 1px rgba(12, 12, 13, 0.1), 0 2px 8px rgba(12, 12, 13, 0.1);
  height: auto;
  margin-bottom: 60px;
  max-width: 100%;
  width: 100%; }

.fixed-container {
  align-items: center;
  display: flex;
  flex-direction: column;
  height: 100vh;
  justify-content: center;
  inset-inline-start: 0;
  margin: 0;
  padding: 0;
  pointer-events: none;
  position: fixed;
  top: 0;
  width: 100%; }

.face-container {
  position: relative;
  width: 64px;
  height: 64px; }

.face {
  width: 62.4px;
  height: 62.4px;
  display: block;
  background-image: url("MOZ_EXTENSION/icons/icon-welcome-face-without-eyes.svg"); }

.eye {
  background-color: #fff;
  width: 10.8px;
  height: 14.6px;
  position: absolute;
  border-radius: 100%;
  overflow: hidden;
  inset-inline-start: 16.4px;
  top: 19.8px; }

.eyeball {
  position: absolute;
  width: 6px;
  height: 6px;
  background-color: #000;
  border-radius: 50%;
  inset-inline-start: 2.4px;
  top: 4.3px;
  z-index: 10; }

.left {
  margin-inline-start: 0; }

.right {
  margin-inline-start: 20px; }

.preview-instructions {
  display: flex;
  align-items: center;
  justify-content: center;
  animation: pulse 125mm cubic-bezier(0.07, 0.95, 0, 1);
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, "segoe ui", "helvetica neue", helvetica, ubuntu, roboto, noto, arial, sans-serif;
  font-size: 24px;
  line-height: 32px;
  text-align: center;
  padding-top: 20px;
  width: 400px;
  user-select: none; }

.cancel-shot {
  background-color: transparent;
  cursor: pointer;
  outline: none;
  border-radius: 3px;
  border: 1px #9b9b9b solid;
  color: #fff;
  cursor: pointer;
  font-family: -apple-system, BlinkMacSystemFont, "segoe ui", "helvetica neue", helvetica, ubuntu, roboto, noto, arial, sans-serif;
  font-size: 16px;
  margin-top: 40px;
  padding: 10px 25px;
  pointer-events: all; }

.all-buttons-container {
  display: flex;
  flex-direction: row-reverse;
  background: #f5f5f5;
  border-radius: 2px;
  box-sizing: border-box;
  height: 80px;
  padding: 8px;
  position: absolute;
  inset-inline-end: 8px;
  top: 8px;
  box-shadow: 0 0 0 1px rgba(12, 12, 13, 0.1), 0 2px 8px rgba(12, 12, 13, 0.1); }
  .all-buttons-container .spacer {
    background-color: #c9c9c9;
    flex: 0 0 1px;
    height: 80px;
    margin: 0 10px;
    position: relative;
    top: -8px; }
  .all-buttons-container button {
    display: flex;
    align-items: center;
    flex-direction: column;
    justify-content: flex-end;
    color: #3e3d40;
    background-color: #f5f5f5;
    background-position: center top;
    background-repeat: no-repeat;
    background-size: 46px 46px;
    border: 1px solid transparent;
    cursor: pointer;
    height: 100%;
    min-width: 90px;
    padding: 46px 5px 5px;
    pointer-events: all;
    transition: border 150ms cubic-bezier(0.07, 0.95, 0, 1), background-color 150ms cubic-bezier(0.07, 0.95, 0, 1);
    white-space: nowrap; }
    .all-buttons-container button:hover {
      background-color: #ebebeb;
      border: 1px solid #c7c7c7; }
    .all-buttons-container button:active {
      background-color: #dedede;
      border: 1px solid #989898; }
  .all-buttons-container .full-page {
    background-image: url("MOZ_EXTENSION/icons/menu-fullpage.svg"); }
  .all-buttons-container .visible {
    background-image: url("MOZ_EXTENSION/icons/menu-visible.svg"); }

@keyframes pulse {
  0% {
    transform: scale(1); }
  50% {
    transform: scale(1.06); }
  100% {
    transform: scale(1); } }

@keyframes fade-in {
  0% {
    opacity: 0; }
  100% {
    opacity: 1; } }

`;
null;
