<div class="content-header">
  <h2 data-i18n="navigationAbout"></h2>
</div>
<p class="small-spacing">
  <strong>
    <span data-manifest="name"></span>, Version: <span data-manifest="version"></span>
  </strong>
</p>
<p class="justify-text">
  <strong data-i18n="aboutLicense"></strong>
  GNU GPLv3
  Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license. Copyright and license notices must be preserved. Contributors provide an express grant of patent rights.
</p>
<hr>
<p>
  <strong data-i18n="aboutContributors"></strong>
  <a target="_blank" href="https://github.com/Robbendebiene/Gesturefy/graphs/contributors">Github</a>,
  <a target="_blank" href="https://crowdin.com/project/gesturefy">Crowdin</a>
</p>
<hr>
<ul class="horizontal-list">
  <li><a target="_blank" href="https://github.com/Robbendebiene/Gesturefy/issues" data-i18n="aboutLinkBug"></a></li>
  <li><a target="_blank" href="https://github.com/Robbendebiene/Gesturefy" data-i18n="aboutLinkCode"></a></li>
  <li><a target="_blank" href="https://addons.mozilla.org/firefox/addon/gesturefy/" data-i18n="aboutLinkAddon"></a></li>
</ul>
<hr>
<div class="align-content-right">
  <a id="backupButton" data-i18n="aboutBackup" class="button"></a>
  <label class="button" for="restoreButton">
    <span data-i18n="aboutRestore"></span>
    <input type="file" id="restoreButton" name="files" accept=".json" hidden>
  </label>
  <button id="resetButton" data-i18n="aboutReset" class="button danger" type="button"></button>
</div>

<popup-box id="restoreAlertNoConfigFile" type="alert">
  <span data-i18n="aboutRestore" slot="title"></span>
  <span data-i18n="aboutRestoreNotificationNoConfigFile" slot="content"></span>
</popup-box>

<popup-box id="restoreAlertWrongFile" type="alert">
  <span data-i18n="aboutRestore" slot="title"></span>
  <span data-i18n="aboutRestoreNotificationWrongFile" slot="content"></span>
</popup-box>

<popup-box id="restoreConfirm" type="confirm">
  <span data-i18n="aboutRestore" slot="title"></span>
  <span data-i18n="aboutRestoreNotificationConfirm" slot="content"></span>
</popup-box>

<popup-box id="restoreAlertSuccess" type="alert">
  <span data-i18n="aboutRestore" slot="title"></span>
  <span data-i18n="aboutRestoreNotificationSuccess" slot="content"></span>
</popup-box>

<popup-box id="resetConfirm" type="confirm">
  <span data-i18n="aboutReset" slot="title"></span>
  <span data-i18n="aboutResetNotificationConfirm" slot="content"></span>
</popup-box>


