<div class="content-header">
  <h2 data-i18n="navigationExtras"></h2>
</div>
<div class="option-list">
  <section class="ol-section">
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelRockerActive" class="ol-name"></p>
        <p data-i18n="extraDescriptionRockerActive" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <input data-config="Settings.Rocker.active" class="toggle-button" type="checkbox">
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelRockerLeftMouseClick" class="ol-name"></p>
      </div>
      <div class="ol-input">
        <command-select data-config="Settings.Rocker.leftMouseClick" class="command-select-field"></command-select>
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelRockerRightMouseClick" class="ol-name"></p>
      </div>
      <div class="ol-input">
        <command-select data-config="Settings.Rocker.rightMouseClick" class="command-select-field"></command-select>
      </div>
    </div>
  </section>
  <section class="ol-section">
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelWheelActive" class="ol-name"></p>
        <p data-i18n="extraDescriptionWheelActive" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <input data-config="Settings.Wheel.active" class="toggle-button" type="checkbox">
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelWheelMouseButton" class="ol-name"></p>
        <p data-i18n="extraDescriptionWheelMouseButton" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <div class="select-wrapper">
          <select data-config="Settings.Wheel.mouseButton" class="select-field">
            <option value="1" data-i18n="settingLabelMouseButtonLeft"></option>
            <option value="2" data-i18n="settingLabelMouseButtonRight"></option>
            <option value="4" data-i18n="settingLabelMouseButtonMiddle"></option>
          </select>
        </div>
      </div>
    </div>
    <div class="ol-item">
      <input id="collapseAdvancedWheelGestureSettings" class="collapseButton" data-collapse="#advancedWheelGestureSettings" type="checkbox">
      <label data-i18n="settingLabelAdvancedSettings" for="collapseAdvancedWheelGestureSettings"></label>
    </div>
    <div id="advancedWheelGestureSettings" class="ol-collapsible hide">
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="extraLabelWheelSensitivity" class="ol-name"></p>
          <p data-i18n="extraDescriptionWheelSensitivity" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Wheel.wheelSensitivity" class="input-field" type="number" required step="1" max="200" min="0">
        </div>
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelWheelUp" class="ol-name"></p>
      </div>
      <div class="ol-input">
        <command-select data-config="Settings.Wheel.wheelUp" class="command-select-field" ></command-select>
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="extraLabelWheelDown" class="ol-name"></p>
      </div>
      <div class="ol-input">
        <command-select data-config="Settings.Wheel.wheelDown" class="command-select-field"></command-select>
      </div>
    </div>
  </section>
</div>
