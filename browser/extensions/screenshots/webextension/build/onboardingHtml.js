/* Created from addon/webextension/onboarding/slides.html */
window.onboardingHtml = `
<!DOCTYPE html>
<html>
  <head>
    <!-- onboarding.scss is automatically inserted here: -->
    <style></style>
    <!-- Here and in onboarding.scss use MOZ_EXTENSION/path to refer to local files -->
  </head>
  <body>
    <div id="slide-overlay">
      <!-- The current slide is set by having .active-slide-1, .active-slide-2, etc on #slide element: -->
      <div id="slide-container" data-number-of-slides="4" class="active-slide-1">
        <div class="slide slide-1">
          <!-- Note: all images must be listed in manifest.json.template under web_accessible_resources -->
          <div class="slide-image" style="background-image: url('MOZ_EXTENSION/icons/onboarding-1.png');"></div>
          <div class="slide-content">
            <div class="slide-content-aligner">
              <h1><span><strong>Firefox</strong> Screenshots</span><sup>Beta</sup></h1>
              <p data-l10n-id="tourBodyOne"></p>
            </div>
            <p class="onboarding-legal-notice"><!-- Substituted with termsAndPrivacyNotice --></p>
          </div>
        </div>
        <div class="slide slide-2">
          <div class="slide-image" style="background-image: url('MOZ_EXTENSION/icons/onboarding-2.png');"></div>
          <div class="slide-content">
            <h1 data-l10n-id="tourHeaderTwo"></h1>
            <p data-l10n-id="tourBodyTwo"></p>
          </div>
        </div>
        <div class="slide slide-3">
          <div class="slide-image" style="background-image: url('MOZ_EXTENSION/icons/onboarding-3.png');"></div>
          <div class="slide-content">
            <h1 data-l10n-id="tourHeaderThree"></h1>
            <p data-l10n-id="tourBodyThree"></p>
          </div>
        </div>
        <div class="slide slide-4">
          <div class="slide-image" style="background-image: url('MOZ_EXTENSION/icons/onboarding-4.png');"></div>
          <div class="slide-content">
            <h1 data-l10n-id="tourHeaderFour"></h1>
            <p data-l10n-id="tourBodyFour"></p>
          </div>
        </div>

        <!-- Clickable elements should be buttons for accessibility -->
        <button id="skip" data-l10n-id="tourSkip" tabindex=1>Skip</button>
        <button id="prev" tabindex=2 data-l10n-label-id="tourPrevious" style="background-image: url('MOZ_EXTENSION/icons/back.svg');"></button>
        <button id="next" tabindex=3 data-l10n-label-id="tourNext" style="background-image: url('MOZ_EXTENSION/icons/back.svg');"/></button>
        <button id="done" tabindex=4 data-l10n-label-id="tourDone" style="background-image: url('MOZ_EXTENSION/icons/done.svg')"></button>
        <div id="slide-status-container">
          <button class="goto-slide goto-slide-1" data-number="1" tabindex=4></button>
          <button class="goto-slide goto-slide-2" data-number="2" tabindex=5></button>
          <button class="goto-slide goto-slide-3" data-number="3" tabindex=6></button>
          <button class="goto-slide goto-slide-4" data-number="4" tabindex=7></button>
        </div>
        <!-- FIXME: Need to put in privacy / etc links -->
      </div>
    </div>
  </body>
</html>

`;
null;

