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
  transition: background 150ms;
  user-select: none;
  white-space: nowrap; }
  .button.small, .small.highlight-button-cancel, .small.highlight-button-save, .small.highlight-button-download {
    height: 32px;
    line-height: 32px;
    padding: 0 8px; }
  .button.tiny, .tiny.highlight-button-cancel, .tiny.highlight-button-save, .tiny.highlight-button-download {
    font-size: 12px;
    height: 22px;
    line-height: 12px;
    padding: 2px 6px; }
  .button.set-width--medium, .set-width--medium.highlight-button-cancel, .set-width--medium.highlight-button-save, .set-width--medium.highlight-button-download {
    max-width: 200px; }
  .button.inline, .inline.highlight-button-cancel, .inline.highlight-button-save, .inline.highlight-button-download {
    display: inline-block; }
  .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-save, .block-button.highlight-button-download {
    display: flex;
    align-items: center;
    justify-content: center;
    border: none;
    border-right: 1px solid rgba(0, 0, 0, 0.1);
    box-shadow: none;
    border-radius: 0;
    height: 100%;
    line-height: 100%;
    padding: 0 20px;
    margin-right: 20px;
    flex: 0 0 155px; }
  .button .arrow-icon, .highlight-button-cancel .arrow-icon, .highlight-button-save .arrow-icon, .highlight-button-download .arrow-icon {
    display: inline-block;
    position: relative;
    top: 1px;
    flex: 0 0 18px;
    height: 16px;
    opacity: .6;
    background-image: url(../img/arrow-page-right-16.svg);
    background-position: right center;
    background-repeat: no-repeat; }

.inverse-color-scheme {
  background: #383E49;
  color: #FFF; }
  .inverse-color-scheme a {
    color: #0996F8; }
  .inverse-color-scheme .large-icon {
    filter: invert(100%); }

.default-color-scheme {
  background: #f2f2f2;
  color: #000; }
  .default-color-scheme a {
    color: #0996F8; }

.button.primary, .primary.highlight-button-cancel, .highlight-button-save, .primary.highlight-button-download {
  background-color: #0996F8;
  color: #FFF; }
  .button.primary:hover, .primary.highlight-button-cancel:hover, .highlight-button-save:hover, .primary.highlight-button-download:hover, .button.primary:focus, .primary.highlight-button-cancel:focus, .highlight-button-save:focus, .primary.highlight-button-download:focus {
    background-color: #0681d7; }
  .button.primary:active, .primary.highlight-button-cancel:active, .highlight-button-save:active, .primary.highlight-button-download:active {
    background-color: #0573be; }

.button.secondary, .highlight-button-cancel, .secondary.highlight-button-save, .highlight-button-download {
  background: #EDEDED;
  color: #000; }
  .button.secondary:hover, .highlight-button-cancel:hover, .secondary.highlight-button-save:hover, .highlight-button-download:hover, .button.secondary:focus, .highlight-button-cancel:focus, .secondary.highlight-button-save:focus, .highlight-button-download:focus {
    background-color: #dbdbdb; }
  .button.secondary:active, .highlight-button-cancel:active, .secondary.highlight-button-save:active, .highlight-button-download:active {
    background-color: #cecece; }

.button.warning, .warning.highlight-button-cancel, .warning.highlight-button-save, .warning.highlight-button-download {
  color: #FFF;
  background: #d92215; }
  .button.warning:hover, .warning.highlight-button-cancel:hover, .warning.highlight-button-save:hover, .warning.highlight-button-download:hover, .button.warning:focus, .warning.highlight-button-cancel:focus, .warning.highlight-button-save:focus, .warning.highlight-button-download:focus {
    background: #b81d12; }
  .button.warning:active, .warning.highlight-button-cancel:active, .warning.highlight-button-save:active, .warning.highlight-button-download:active {
    background: #a11910; }

.subtitle-link {
  color: #0996F8; }

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
  background: rgba(255, 255, 255, 0.1);
  border-radius: 1px;
  pointer-events: none;
  position: absolute;
  z-index: 10000000000; }

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
  background-color: #FFF;
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

.highlight-button-cancel {
  background-color: #ededed;
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
  background-color: #ededed;
  background-image: url("MOZ_EXTENSION/icons/download.svg");
  background-position: center center;
  background-repeat: no-repeat;
  background-size: 18px 18px;
  display: block;
  margin: 5px;
  width: 40px; }

.highlight-button-cancel,
.highlight-button-download {
  transition: background-color cubic-bezier(0.07, 0.95, 0, 1) 250ms; }
  .highlight-button-cancel:hover, .highlight-button-cancel:focus, .highlight-button-cancel:active,
  .highlight-button-download:hover,
  .highlight-button-download:focus,
  .highlight-button-download:active {
    background-color: #dbdbdb; }

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
  height: 100%;
  justify-content: center;
  left: 0;
  margin: 0;
  padding: 0;
  pointer-events: none;
  position: absolute;
  top: 0;
  width: 100%; }

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

/* styleMyShotsButton test: */
.styleMyShotsButton-bright .myshots-button {
  color: #fff;
  background: #0996F8; }

.styleMyShotsButton-bright .myshots-text-pre,
.styleMyShotsButton-bright .myshots-text-post {
  filter: brightness(20); }

/* end styleMyShotsButton test */
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

