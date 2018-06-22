/* Created from build/server/static/css/inline-selection.css */
window.inlineSelectionCss = `
.button, .highlight-button-cancel, .highlight-button-save, .highlight-button-download, .highlight-button-copy, .preview-button-save {
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
  .button.small, .small.highlight-button-cancel, .small.highlight-button-save, .small.highlight-button-download, .small.highlight-button-copy, .small.preview-button-save {
    height: 32px;
    line-height: 32px;
    padding: 0 8px; }
  .button.active, .active.highlight-button-cancel, .active.highlight-button-save, .active.highlight-button-download, .active.highlight-button-copy, .active.preview-button-save {
    background-color: #dedede; }
  .button.tiny, .tiny.highlight-button-cancel, .tiny.highlight-button-save, .tiny.highlight-button-download, .tiny.highlight-button-copy, .tiny.preview-button-save {
    font-size: 14px;
    height: 26px;
    border: 1px solid #c7c7c7; }
    .button.tiny:hover, .tiny.highlight-button-cancel:hover, .tiny.highlight-button-save:hover, .tiny.highlight-button-download:hover, .tiny.highlight-button-copy:hover, .tiny.preview-button-save:hover, .button.tiny:focus, .tiny.highlight-button-cancel:focus, .tiny.highlight-button-save:focus, .tiny.highlight-button-download:focus, .tiny.highlight-button-copy:focus, .tiny.preview-button-save:focus {
      background: #ededf0;
      border-color: #989898; }
    .button.tiny:active, .tiny.highlight-button-cancel:active, .tiny.highlight-button-save:active, .tiny.highlight-button-download:active, .tiny.highlight-button-copy:active, .tiny.preview-button-save:active {
      background: #dedede;
      border-color: #989898; }
  .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-save, .block-button.highlight-button-download, .block-button.highlight-button-copy, .block-button.preview-button-save {
    display: flex;
    align-items: center;
    justify-content: center;
    box-sizing: border-box;
    border: 0;
    border-right: 1px solid #c7c7c7;
    box-shadow: 0;
    border-radius: 0;
    flex-shrink: 0;
    font-size: 20px;
    height: 100px;
    line-height: 100%;
    overflow: hidden; }
    @media (max-width: 719px) {
      .button.block-button, .block-button.highlight-button-cancel, .block-button.highlight-button-save, .block-button.highlight-button-download, .block-button.highlight-button-copy, .block-button.preview-button-save {
        justify-content: flex-start;
        font-size: 16px;
        height: 72px;
        margin-right: 10px;
        padding: 0 5px; } }
    .button.block-button:hover, .block-button.highlight-button-cancel:hover, .block-button.highlight-button-save:hover, .block-button.highlight-button-download:hover, .block-button.highlight-button-copy:hover, .block-button.preview-button-save:hover {
      background: #ededf0; }
    .button.block-button:active, .block-button.highlight-button-cancel:active, .block-button.highlight-button-save:active, .block-button.highlight-button-download:active, .block-button.highlight-button-copy:active, .block-button.preview-button-save:active {
      background: #dedede; }
  .button.download, .download.highlight-button-cancel, .download.highlight-button-save, .download.highlight-button-download, .download.highlight-button-copy, .download.preview-button-save, .button.edit, .edit.highlight-button-cancel, .edit.highlight-button-save, .edit.highlight-button-download, .edit.highlight-button-copy, .edit.preview-button-save, .button.trash, .trash.highlight-button-cancel, .trash.highlight-button-save, .trash.highlight-button-download, .trash.highlight-button-copy, .trash.preview-button-save, .button.share, .share.highlight-button-cancel, .share.highlight-button-save, .share.highlight-button-download, .share.highlight-button-copy, .share.preview-button-save, .button.flag, .flag.highlight-button-cancel, .flag.highlight-button-save, .flag.highlight-button-download, .flag.highlight-button-copy, .flag.preview-button-save {
    background-repeat: no-repeat;
    background-size: 50%;
    background-position: center;
    margin-right: 10px;
    transition: background-color 150ms cubic-bezier(0.07, 0.95, 0, 1); }
  .button.download, .download.highlight-button-cancel, .download.highlight-button-save, .download.highlight-button-download, .download.highlight-button-copy, .download.preview-button-save {
    background-image: url("../img/icon-download.svg"); }
    .button.download:hover, .download.highlight-button-cancel:hover, .download.highlight-button-save:hover, .download.highlight-button-download:hover, .download.highlight-button-copy:hover, .download.preview-button-save:hover {
      background-color: #ededf0; }
    .button.download:active, .download.highlight-button-cancel:active, .download.highlight-button-save:active, .download.highlight-button-download:active, .download.highlight-button-copy:active, .download.preview-button-save:active {
      background-color: #dedede; }
  .button.share, .share.highlight-button-cancel, .share.highlight-button-save, .share.highlight-button-download, .share.highlight-button-copy, .share.preview-button-save {
    background-image: url("../img/icon-share.svg"); }
    .button.share:hover, .share.highlight-button-cancel:hover, .share.highlight-button-save:hover, .share.highlight-button-download:hover, .share.highlight-button-copy:hover, .share.preview-button-save:hover {
      background-color: #ededf0; }
    .button.share.active, .share.active.highlight-button-cancel, .share.active.highlight-button-save, .share.active.highlight-button-download, .share.active.highlight-button-copy, .share.active.preview-button-save, .button.share:active, .share.highlight-button-cancel:active, .share.highlight-button-save:active, .share.highlight-button-download:active, .share.highlight-button-copy:active, .share.preview-button-save:active {
      background-color: #dedede; }
  .button.share.newicon, .share.newicon.highlight-button-cancel, .share.newicon.highlight-button-save, .share.newicon.highlight-button-download, .share.newicon.highlight-button-copy, .share.newicon.preview-button-save {
    background-image: url("../img/icon-share-alternate.svg"); }
  .button.trash, .trash.highlight-button-cancel, .trash.highlight-button-save, .trash.highlight-button-download, .trash.highlight-button-copy, .trash.preview-button-save {
    background-image: url("../img/icon-trash.svg"); }
    .button.trash:hover, .trash.highlight-button-cancel:hover, .trash.highlight-button-save:hover, .trash.highlight-button-download:hover, .trash.highlight-button-copy:hover, .trash.preview-button-save:hover {
      background-color: #ededf0; }
    .button.trash:active, .trash.highlight-button-cancel:active, .trash.highlight-button-save:active, .trash.highlight-button-download:active, .trash.highlight-button-copy:active, .trash.preview-button-save:active {
      background-color: #dedede; }
  .button.edit, .edit.highlight-button-cancel, .edit.highlight-button-save, .edit.highlight-button-download, .edit.highlight-button-copy, .edit.preview-button-save {
    background-image: url("../img/icon-edit.svg"); }
    .button.edit:hover, .edit.highlight-button-cancel:hover, .edit.highlight-button-save:hover, .edit.highlight-button-download:hover, .edit.highlight-button-copy:hover, .edit.preview-button-save:hover {
      background-color: #ededf0; }
    .button.edit:active, .edit.highlight-button-cancel:active, .edit.highlight-button-save:active, .edit.highlight-button-download:active, .edit.highlight-button-copy:active, .edit.preview-button-save:active {
      background-color: #dedede; }
  .button.flag, .flag.highlight-button-cancel, .flag.highlight-button-save, .flag.highlight-button-download, .flag.highlight-button-copy, .flag.preview-button-save {
    background-image: url("../img/icon-flag.svg"); }
    .button.flag:hover, .flag.highlight-button-cancel:hover, .flag.highlight-button-save:hover, .flag.highlight-button-download:hover, .flag.highlight-button-copy:hover, .flag.preview-button-save:hover {
      background-color: #ededf0; }
    .button.flag:active, .flag.highlight-button-cancel:active, .flag.highlight-button-save:active, .flag.highlight-button-download:active, .flag.highlight-button-copy:active, .flag.preview-button-save:active {
      background-color: #dedede; }

.inverse-color-scheme {
  background: #38383d;
  color: #f9f9fa; }
  .inverse-color-scheme a {
    color: #e1e1e6; }

.default-color-scheme {
  background: #f9f9fa;
  color: #38383d; }
  .default-color-scheme a {
    color: #009ec0; }

.highlight-color-scheme {
  background: #009ec0;
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

.button.primary, .primary.highlight-button-cancel, .highlight-button-save, .primary.highlight-button-download, .primary.highlight-button-copy, .preview-button-save {
  background-color: #009ec0;
  color: #fff; }
  .button.primary:hover, .primary.highlight-button-cancel:hover, .highlight-button-save:hover, .primary.highlight-button-download:hover, .primary.highlight-button-copy:hover, .preview-button-save:hover, .button.primary:focus, .primary.highlight-button-cancel:focus, .highlight-button-save:focus, .primary.highlight-button-download:focus, .primary.highlight-button-copy:focus, .preview-button-save:focus {
    background-color: #00819c; }
  .button.primary:active, .primary.highlight-button-cancel:active, .highlight-button-save:active, .primary.highlight-button-download:active, .primary.highlight-button-copy:active, .preview-button-save:active {
    background-color: #006c83; }

.button.secondary, .highlight-button-cancel, .secondary.highlight-button-save, .highlight-button-download, .highlight-button-copy, .secondary.preview-button-save {
  background-color: #f9f9fa;
  color: #38383d; }
  .button.secondary:hover, .highlight-button-cancel:hover, .secondary.highlight-button-save:hover, .highlight-button-download:hover, .highlight-button-copy:hover, .secondary.preview-button-save:hover {
    background-color: #ededf0; }
  .button.secondary:active, .highlight-button-cancel:active, .secondary.highlight-button-save:active, .highlight-button-download:active, .highlight-button-copy:active, .secondary.preview-button-save:active {
    background-color: #dedede; }

.button.transparent, .transparent.highlight-button-cancel, .transparent.highlight-button-save, .transparent.highlight-button-download, .transparent.highlight-button-copy, .transparent.preview-button-save {
  background-color: transparent;
  color: #38383d; }
  .button.transparent:hover, .transparent.highlight-button-cancel:hover, .transparent.highlight-button-save:hover, .transparent.highlight-button-download:hover, .transparent.highlight-button-copy:hover, .transparent.preview-button-save:hover {
    background-color: #ededf0; }
  .button.transparent:focus, .transparent.highlight-button-cancel:focus, .transparent.highlight-button-save:focus, .transparent.highlight-button-download:focus, .transparent.highlight-button-copy:focus, .transparent.preview-button-save:focus, .button.transparent:active, .transparent.highlight-button-cancel:active, .transparent.highlight-button-save:active, .transparent.highlight-button-download:active, .transparent.highlight-button-copy:active, .transparent.preview-button-save:active {
    background-color: #dedede; }

.button.warning, .warning.highlight-button-cancel, .warning.highlight-button-save, .warning.highlight-button-download, .warning.highlight-button-copy, .warning.preview-button-save {
  color: #fff;
  background: #d92215; }
  .button.warning:hover, .warning.highlight-button-cancel:hover, .warning.highlight-button-save:hover, .warning.highlight-button-download:hover, .warning.highlight-button-copy:hover, .warning.preview-button-save:hover, .button.warning:focus, .warning.highlight-button-cancel:focus, .warning.highlight-button-save:focus, .warning.highlight-button-download:focus, .warning.highlight-button-copy:focus, .warning.preview-button-save:focus {
    background: #b81d12; }
  .button.warning:active, .warning.highlight-button-cancel:active, .warning.highlight-button-save:active, .warning.highlight-button-download:active, .warning.highlight-button-copy:active, .warning.preview-button-save:active {
    background: #a11910; }

.subtitle-link {
  color: #009ec0; }

.loader {
  background: #2e2d30;
  border-radius: 2px;
  height: 4px;
  overflow: hidden;
  position: relative;
  width: 200px; }
  #shot-index .loader {
    background-color: #dedede; }

.loader-inner {
  animation: bounce infinite alternate 1250ms cubic-bezier(0.7, 0, 0.3, 1);
  background: #04d1e6;
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
    left: 0;
    position: absolute;
    right: 0;
    top: 0; }
  body.hcm .hover-highlight {
    background-color: white;
    opacity: 0.2; }

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
  body.hcm .bghighlight {
    background-color: black;
    opacity: 0.7; }

.notice {
  display: flex;
  height: 41px;
  left: 50%;
  position: fixed;
  transform: translate(-50%, 0);
  transition: top 125ms ease-out, translate 125ms ease-out;
  user-select: none;
  will-change: top, translate;
  z-index: 10000000000; }
  .notice .notice-wrapper {
    align-items: center;
    background: #737373;
    border-radius: 100px;
    display: flex;
    flex-wrap: nowrap;
    justify-content: space-between;
    padding: 10px 15px; }
  .notice .notice-content {
    color: #fff;
    flex: 1;
    font-family: -apple-system, BlinkMacSystemFont, "segoe ui", "helvetica neue", helvetica, ubuntu, roboto, noto, arial, sans-serif;
    font-size: 14px;
    font-weight: bold;
    white-space: nowrap; }
  .notice .notice-help {
    background-image: url("MOZ_EXTENSION/icons/help-16.svg");
    background-position: center center;
    background-repeat: no-repeat;
    height: 16px;
    width: 16px; }

.notice-tooltip {
  background: #fff;
  border-radius: 3px;
  border: 1px solid #9d9d9e;
  bottom: 60px;
  color: #000;
  cursor: default;
  display: none;
  font-family: -apple-system, BlinkMacSystemFont, "segoe ui", "helvetica neue", helvetica, ubuntu, roboto, noto, arial, sans-serif;
  font-size: 14px;
  font-weight: normal;
  line-height: 22px;
  overflow-wrap: break-word;
  padding: 15px;
  position: absolute;
  right: -14px;
  white-space: normal;
  width: 300px;
  z-index: 10000000000;
  /* down-arrow for the tooltip */ }
  .notice:hover .notice-tooltip {
    display: block; }
  .notice-tooltip p {
    margin: 0; }
  .notice-tooltip ul {
    margin-bottom: 0; }
  .notice-tooltip::after {
    border-left: 10px solid transparent;
    border-right: 10px solid transparent;
    border-top: 10px solid #f9f9fa;
    content: "";
    height: 0;
    left: 86%;
    position: absolute;
    top: 100%;
    width: 0; }

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
  body.hcm .preview-overlay {
    background-color: black;
    opacity: 0.7; }

.precision-cursor {
  cursor: crosshair; }

.highlight {
  border-radius: 2px;
  border: 2px dashed rgba(255, 255, 255, 0.8);
  box-sizing: border-box;
  cursor: move;
  position: absolute;
  z-index: 9999999999; }
  body.hcm .highlight {
    border: 2px dashed white;
    opacity: 1.0; }

.highlight-buttons {
  display: flex;
  align-items: center;
  justify-content: center;
  bottom: -55px;
  position: absolute;
  z-index: 6; }
  html[dir="rtl"] .highlight-buttons {
    left: 5px; }
  html[dir="ltr"] .highlight-buttons {
    right: 5px; }
  .bottom-selection .highlight-buttons {
    bottom: 5px; }
  .left-selection .highlight-buttons {
    right: auto;
    left: 5px; }

.highlight-button-cancel {
  border: 1px solid #dedede;
  margin: 5px;
  width: 40px; }

.highlight-button-save {
  font-size: 18px;
  margin: 5px;
  min-width: 80px; }
  html[dir="ltr"] .highlight-button-save img {
    padding-right: 8px; }
  html[dir="rtl"] .highlight-button-save img {
    padding-left: 8px; }

.highlight-button-download {
  border: 1px solid #dedede;
  display: block;
  margin: 5px;
  width: 40px; }
  .highlight-button-download.download-only-button {
    font-size: 18px;
    width: auto; }
    .highlight-button-download.download-only-button img {
      height: 16px;
      width: 16px; }
      html[dir="ltr"] .highlight-button-download.download-only-button img {
        padding-right: 8px; }
      html[dir="rtl"] .highlight-button-download.download-only-button img {
        padding-left: 8px; }

.highlight-button-copy {
  border: 1px solid #dedede;
  display: block;
  margin: 5px;
  width: 40px; }

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
  justify-content: center;
  position: absolute;
  top: -2px; }
  html[dir="rtl"] .preview-buttons {
    left: 0; }
  html[dir="ltr"] .preview-buttons {
    right: 0; }

.preview-image {
  position: relative;
  height: 80%;
  max-width: 100%;
  margin: auto 2em;
  text-align: center;
  animation-delay: 50ms;
  animation: bounce-in 300ms forwards ease-in-out; }

.preview-image > img {
  display: block;
  width: auto;
  height: auto;
  max-width: 100%;
  max-height: 90%;
  margin: 50px auto;
  border: 1px solid rgba(255, 255, 255, 0.8); }

.preview-button-save {
  font-size: 18px;
  margin: 5px;
  min-width: 80px; }
  html[dir="ltr"] .preview-button-save img {
    padding-right: 8px; }
  html[dir="rtl"] .preview-button-save img {
    padding-left: 8px; }

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

.myshots-all-buttons-container {
  display: flex;
  flex-direction: row-reverse;
  background: #f5f5f5;
  border-radius: 2px;
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

