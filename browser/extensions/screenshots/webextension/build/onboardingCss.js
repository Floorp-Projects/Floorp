/* Created from build/server/static/css/onboarding.css */
window.onboardingCss = `
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

html,
body {
  box-sizing: border-box;
  font-family: -apple-system, BlinkMacSystemFont, sans-serif;
  height: 100%;
  margin: 0;
  width: 100%; }

#slide-overlay {
  display: flex;
  align-items: center;
  flex-direction: column;
  justify-content: center;
  animation: fade-in 250ms forwards cubic-bezier(0.07, 0.95, 0, 1);
  background: rgba(0, 0, 0, 0.8);
  height: 100%;
  opacity: 0;
  width: 100%; }

#slide-container {
  animation-delay: 50ms;
  animation: fade-in 250ms forwards cubic-bezier(0.07, 0.95, 0, 1);
  opacity: 0; }

.slide {
  display: flex;
  align-items: center;
  flex-direction: column;
  justify-content: center;
  background-color: #f5f5f7;
  border-radius: 5px;
  height: 520px;
  overflow: hidden;
  width: 700px; }
  .slide .slide-image {
    background-size: 700px 378px;
    flex: 0 0 360px;
    font-size: 16px;
    width: 100%; }
  .slide .slide-content {
    display: flex;
    align-items: center;
    flex-direction: column;
    justify-content: center;
    box-sizing: border-box;
    flex: 0 0 160px;
    padding: 5px;
    text-align: center; }
  .slide h1 {
    font-size: 30px;
    font-weight: 400;
    margin: 0 0 10px; }
    .slide h1 sup {
      background: #00d1e6;
      border-radius: 2px;
      color: #fff;
      font-size: 16px;
      margin-left: 5px;
      padding: 2px; }
  .slide p {
    animation-duration: 350ms;
    font-size: 16px;
    line-height: 23px;
    margin: 0;
    width: 75%; }
  .slide .slide-content-aligner h1 {
    font-size: 34px; }
  .slide .slide-content-aligner p {
    margin: 0 auto; }
  .slide .onboarding-legal-notice {
    font-size: 12px;
    color: #858585; }
    .slide .onboarding-legal-notice a {
      color: #009ec0;
      text-decoration: none; }
  .slide:not(.slide-1) h1 {
    opacity: 0;
    transform: translate3d(160px, 0, 0);
    animation: slide-left 500ms forwards cubic-bezier(0.07, 0.95, 0, 1); }
  .slide:not(.slide-1) p {
    opacity: 0;
    transform: translate3d(160px, 0, 0);
    animation: slide-left 600ms forwards cubic-bezier(0.07, 0.95, 0, 1); }
  .slide:not(.slide-1) .slide-image {
    background-color: #00d1e6; }
  .slide.slide-1 {
    background: #fff; }
    .slide.slide-1 .slide-content {
      justify-content: space-between;
      width: 100%; }

.slide-1,
.slide-2,
.slide-3,
.slide-4,
.slide-5 {
  display: none; }

.active-slide-1 .slide-1,
.active-slide-2 .slide-2,
.active-slide-3 .slide-3,
.active-slide-4 .slide-4 {
  display: flex; }

#slide-status-container {
  display: flex;
  align-items: center;
  justify-content: center;
  padding-top: 15px; }

.goto-slide {
  background: transparent;
  background-color: #f5f5f7;
  border-radius: 50%;
  border: 0;
  flex: 0 0 9px;
  height: 9px;
  margin: 0 4px;
  opacity: 0.7;
  padding: 0;
  transition: height 100ms cubic-bezier(0.07, 0.95, 0, 1), opacity 100ms cubic-bezier(0.07, 0.95, 0, 1); }

.goto-slide:hover {
  opacity: 1; }

.active-slide-1 .goto-slide-1,
.active-slide-2 .goto-slide-2,
.active-slide-3 .goto-slide-3,
.active-slide-4 .goto-slide-4 {
  opacity: 1;
  transform: scale(1.1); }

#prev, #next,
#done {
  background-color: #f0f0f0;
  border-radius: 50%;
  border: 0;
  box-shadow: 0 0 12px rgba(0, 0, 0, 0.2);
  display: inline-block;
  height: 70px;
  margin-top: -70px;
  position: absolute;
  text-align: center;
  top: 50%;
  transition: background-color 150ms cubic-bezier(0.07, 0.95, 0, 1), background-size 250ms cubic-bezier(0.07, 0.95, 0, 1);
  width: 70px; }

#prev {
  background-image: url("MOZ_EXTENSION/icons/back.svg");
  left: 50%;
  margin-left: -385px; }

#next,
#done {
  left: 50%;
  margin-left: 315px; }

#prev,
#next,
#done {
  background-position: center center;
  background-repeat: no-repeat;
  background-size: 20px 20px; }
  #prev:hover,
  #next:hover,
  #done:hover {
    background-color: #fff;
    background-size: 22px 22px; }
  #prev:active,
  #next:active,
  #done:active {
    background-color: #fff;
    background-size: 24px 24px; }

#next {
  background-image: url("MOZ_EXTENSION/icons/back.svg");
  transform: rotate(180deg); }
  .active-slide-1 #next {
    background-image: url("MOZ_EXTENSION/icons/back-highlight.svg"); }

#skip {
  background: none;
  border: 0;
  color: #fff;
  font-size: 16px;
  left: 50%;
  margin-left: -330px;
  margin-top: 257px;
  opacity: 0.7;
  position: absolute;
  top: 50%;
  transition: opacity 100ms cubic-bezier(0.07, 0.95, 0, 1);
  z-index: 10; }

#skip:hover {
  opacity: 1; }

.active-slide-1 #prev,
.active-slide-4 #next {
  display: none; }

#done {
  background-image: url("MOZ_EXTENSION/icons/done.svg");
  display: none; }

.active-slide-4 #done {
  display: inline-block; }

/* for smaller screen sizes */
@media screen and (max-width: 768px) {
  .slide {
    height: 360px;
    width: 450px; }
    .slide .slide-image {
      background-size: contain;
      background-repeat: no-repeat;
      background-position: center;
      flex: 0 0 200px; }
    .slide .slide-content {
      flex: 0 0 160px; }
      .slide .slide-content h1 {
        font-size: 24px; }
      .slide .slide-content p {
        font-size: 14px;
        line-height: 21px;
        width: 85%; }
      .slide .slide-content .onboarding-legal-notice {
        font-size: 10px;
        line-height: 16px; }
  #skip {
    margin-left: -205px;
    margin-top: 177px; }
  #prev {
    margin-left: -260px; }
  #next,
  #done {
    margin-left: 190px; } }

`;
null;

