/* Created from build/server/static/css/inline-selection.css */
window.inlineSelectionCss = `
.button, .highlight-button-cancel, .highlight-button-save, .highlight-button-download {
  display: flex;
  align-items: center;
  justify-content: center;
  border: 0;
  border-radius: 1px;
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
  .button.small, .small.highlight-button-cancel, .small.highlight-button-save, .small.highlight-button-download {
    height: 32px;
    line-height: 32px;
    padding: 0 8px; }
  .button.tiny, .tiny.highlight-button-cancel, .tiny.highlight-button-save, .tiny.highlight-button-download {
    font-size: 14px;
    height: 26px;
    border: 1px solid #c7c7c7; }
    .button.tiny:hover, .tiny.highlight-button-cancel:hover, .tiny.highlight-button-save:hover, .tiny.highlight-button-download:hover, .button.tiny:focus, .tiny.highlight-button-cancel:focus, .tiny.highlight-button-save:focus, .tiny.highlight-button-download:focus {
      background: #ebebeb;
      border-color: #989898; }
    .button.tiny:active, .tiny.highlight-button-cancel:active, .tiny.highlight-button-save:active, .tiny.highlight-button-download:active {
      background: #dedede;
      border-color: #989898; }
  .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-save, .block-button.highlight-button-download {
    display: flex;
    align-items: center;
    justify-content: center;
    box-sizing: border-box;
    border: none;
    border-right: 1px solid #c7c7c7;
    box-shadow: none;
    border-radius: 0;
    flex-shrink: 0;
    font-size: 20px;
    height: 100px;
    line-height: 100%;
    overflow: hidden; }
    @media (max-width: 719px) {
      .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-save, .block-button.highlight-button-download {
        justify-content: flex-start;
        font-size: 16px;
        height: 72px;
        margin-right: 10px;
        padding: 0 5px; } }
    .button.block-button:hover, .block-button.highlight-button-cancel:hover, .block-button.highlight-button-save:hover, .block-button.highlight-button-download:hover {
      background: #ebebeb; }
    .button.block-button:active, .block-button.highlight-button-cancel:active, .block-button.highlight-button-save:active, .block-button.highlight-button-download:active {
      background: #dedede; }

.inverse-color-scheme {
  background: #3e3d40;
  color: #f5f5f7; }
  .inverse-color-scheme a {
    color: #e1e1e6; }

.default-color-scheme {
  background: #f5f5f7;
  color: #3e3d40; }
  .default-color-scheme a {
    color: #009ec0; }

.highlight-color-scheme {
  background: #009ec0;
  color: #fff; }
  .highlight-color-scheme a {
    color: #fff;
    text-decoration: underline; }

.alt-color-scheme {
  background: #31365A;
  color: #f5f5f7; }
  .alt-color-scheme h1 {
    color: #6F7FB6; }
  .alt-color-scheme a {
    color: #e1e1e6;
    text-decoration: underline; }

.button.primary, .primary.highlight-button-cancel, .highlight-button-save, .primary.highlight-button-download {
  background-color: #009ec0;
  color: #fff; }
  .button.primary:hover, .primary.highlight-button-cancel:hover, .highlight-button-save:hover, .primary.highlight-button-download:hover, .button.primary:focus, .primary.highlight-button-cancel:focus, .highlight-button-save:focus, .primary.highlight-button-download:focus {
    background-color: #00819c; }
  .button.primary:active, .primary.highlight-button-cancel:active, .highlight-button-save:active, .primary.highlight-button-download:active {
    background-color: #006c83; }

.button.secondary, .highlight-button-cancel, .secondary.highlight-button-save, .highlight-button-download {
  background-color: #f5f5f7;
  color: #3e3d40; }
  .button.secondary:hover, .highlight-button-cancel:hover, .secondary.highlight-button-save:hover, .highlight-button-download:hover {
    background-color: #ebebeb; }
  .button.secondary:hover, .highlight-button-cancel:hover, .secondary.highlight-button-save:hover, .highlight-button-download:hover {
    background-color: #dedede; }

.button.transparent, .transparent.highlight-button-cancel, .transparent.highlight-button-save, .transparent.highlight-button-download {
  background-color: transparent;
  color: #3e3d40; }
  .button.transparent:hover, .transparent.highlight-button-cancel:hover, .transparent.highlight-button-save:hover, .transparent.highlight-button-download:hover, .button.transparent:focus, .transparent.highlight-button-cancel:focus, .transparent.highlight-button-save:focus, .transparent.highlight-button-download:focus, .button.transparent:active, .transparent.highlight-button-cancel:active, .transparent.highlight-button-save:active, .transparent.highlight-button-download:active {
    background-color: rgba(0, 0, 0, 0.05); }

.button.warning, .warning.highlight-button-cancel, .warning.highlight-button-save, .warning.highlight-button-download {
  color: #fff;
  background: #d92215; }
  .button.warning:hover, .warning.highlight-button-cancel:hover, .warning.highlight-button-save:hover, .warning.highlight-button-download:hover, .button.warning:focus, .warning.highlight-button-cancel:focus, .warning.highlight-button-save:focus, .warning.highlight-button-download:focus {
    background: #b81d12; }
  .button.warning:active, .warning.highlight-button-cancel:active, .warning.highlight-button-save:active, .warning.highlight-button-download:active {
    background: #a11910; }

.subtitle-link {
  color: #009ec0; }

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
    opacity: .3;
    transform: scale(1); }
  70% {
    opacity: .25;
    transform: scale(1.04); }
  100% {
    opacity: .3;
    transform: scale(1); } }

@keyframes slide-left {
  0% {
    opacity: 0;
    transform: translate3d(160px, 0, 0); }
  100% {
    opacity: 1;
    transform: translate3d(0, 0, 0); } }

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
  .hover-highlight:before {
    border: 2px dashed rgba(255, 255, 255, 0.4);
    bottom: 0;
    content: '';
    left: 0;
    position: absolute;
    right: 0;
    top: 0; }

.mover-target.direction-topLeft {
  cursor: nwse-resize;
  height: 60px;
  left: -30px;
  top: -30px;
  width: 60px; }

.mover-target.direction-top {
  cursor: ns-resize;
  height: 60px;
  left: 0;
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
  left: 0;
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

.preview-overlay {
  align-items: center;
  background-color: rgba(0, 0, 0, 0.7);
  display: flex;
  height: 100%;
  justify-content: center;
  left: 0;
  margin: 0;
  padding: 0;
  position: fixed;
  top: 0;
  width: 100%;
  z-index: 9999999999; }

.highlight {
  border-radius: 2px;
  border: 2px dashed rgba(255, 255, 255, 0.8);
  box-sizing: border-box;
  cursor: move;
  position: absolute;
  z-index: 9999999999; }

.highlight-buttons {
  display: flex;
  align-items: center;
  justify-content: center;
  bottom: -55px;
  position: absolute;
  right: 5px;
  z-index: 6; }
  .bottom-selection .highlight-buttons {
    bottom: 5px; }
  .left-selection .highlight-buttons {
    right: auto;
    left: 5px; }

.highlight-button-cancel {
  background-image: url("MOZ_EXTENSION/icons/cancel.svg");
  background-position: center center;
  background-repeat: no-repeat;
  background-size: 18px 18px;
  margin: 5px;
  width: 40px; }

.highlight-button-save {
  font-size: 18px;
  margin: 5px;
  min-width: 80px; }

.highlight-button-download {
  background-image: url("MOZ_EXTENSION/icons/download.svg");
  background-position: center center;
  background-repeat: no-repeat;
  background-size: 18px 18px;
  display: block;
  margin: 5px;
  width: 40px; }

.pixel-dimensions {
  position: absolute;
  pointer-events: none;
  font-weight: bold;
  font-family: sans-serif;
  font-size: 70%;
  color: #000;
  text-shadow: -1px -1px 0 #fff, 1px -1px 0 #fff, -1px 1px 0 #fff, 1px 1px 0 #fff; }

.fixed-container {
  align-items: center;
  display: flex;
  flex-direction: column;
  height: 100vh;
  justify-content: center;
  left: 0;
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
  left: 16.4px;
  top: 19.8px; }

.eyeball {
  position: absolute;
  width: 6px;
  height: 6px;
  background-color: #000;
  border-radius: 50%;
  left: 2.4px;
  top: 4.3px;
  z-index: 10; }

.left {
  margin-left: 0; }

.right {
  margin-left: 20px; }

.preview-instructions {
  display: flex;
  align-items: center;
  justify-content: center;
  animation: pulse 125mm cubic-bezier(0.07, 0.95, 0, 1);
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, sans-serif;
  font-size: 24px;
  line-height: 32px;
  text-align: center;
  padding-top: 20px;
  width: 400px; }

.myshots-all-buttons-container {
  display: flex;
  flex-direction: row-reverse;
  background: #f5f5f5;
  border-radius: 1px;
  box-sizing: border-box;
  height: 80px;
  padding: 8px;
  position: absolute;
  right: 5px;
  top: 5px; }
  html[dir="rtl"] .myshots-all-buttons-container {
    left: 5px;
    right: inherit; }
  .myshots-all-buttons-container .spacer {
    background-color: #c9c9c9;
    flex: 0 0 1px;
    height: 80px;
    margin: 0 10px;
    position: relative;
    top: -8px; }
  .myshots-all-buttons-container button {
    display: flex;
    align-items: center;
    flex-direction: column;
    justify-content: flex-end;
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
    .myshots-all-buttons-container button:hover {
      background-color: #ebebeb;
      border: 1px solid #c7c7c7; }
    .myshots-all-buttons-container button:active {
      background-color: #dedede;
      border: 1px solid #989898; }
  .myshots-all-buttons-container .myshots-button {
    background-image: url("MOZ_EXTENSION/icons/menu-myshot.svg"); }
  .myshots-all-buttons-container .full-page {
    background-image: url("MOZ_EXTENSION/icons/menu-fullpage.svg"); }
  .myshots-all-buttons-container .visible {
    background-image: url("MOZ_EXTENSION/icons/menu-visible.svg"); }

.myshots-button-container {
  display: flex;
  align-items: center;
  justify-content: center; }

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

