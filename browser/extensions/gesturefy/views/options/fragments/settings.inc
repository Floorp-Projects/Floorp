<div class="content-header">
  <h2 data-i18n="navigationSettings"></h2>
</div>
<div class="option-list">
  <section class="ol-section">
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="settingLabelMouseButton" class="ol-name"></p>
        <p data-i18n="settingDescriptionMouseButton" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <div class="select-wrapper">
          <select data-config="Settings.Gesture.mouseButton" class="select-field">
            <option value="1" data-i18n="settingLabelMouseButtonLeft"></option>
            <option value="2" data-i18n="settingLabelMouseButtonRight"></option>
            <option value="4" data-i18n="settingLabelMouseButtonMiddle"></option>
          </select>
        </div>
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="settingLabelSuppressionKey" class="ol-name"></p>
        <p data-i18n="settingDescriptionSuppressionKey" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <div class="select-wrapper">
          <select data-config="Settings.Gesture.suppressionKey" class="select-field">
            <option value="" data-i18n="settingLabelNoneKey"></option>
            <option value="altKey" data-i18n="settingLabelAltKey"></option>
            <option value="ctrlKey" data-i18n="settingLabelCtrlKey"></option>
            <option value="shiftKey" data-i18n="settingLabelShiftKey"></option>
          </select>
        </div>
      </div>
    </div>
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="settingLabelTimeoutActive" class="ol-name"></p>
        <p data-i18n="settingDescriptionTimeoutActive" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <input data-collapse="#timeoutSettings" data-config="Settings.Gesture.Timeout.active" class="toggle-button" type="checkbox">
      </div>
    </div>
    <div id="timeoutSettings" class="ol-collapsible">
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelTimeoutDuration" class="ol-name"></p>
          <p data-i18n="settingDescriptionTimeoutDuration" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.Timeout.duration" class="input-field" type="number" step="0.001" min="0.001" max="10" required>
        </div>
      </div>
    </div>
    <div class="ol-item">
      <input id="collapseAdvancedMouseGestureSettings" class="collapseButton" data-collapse="#advancedMouseGestureSettings" type="checkbox">
      <label data-i18n="settingLabelAdvancedSettings" for="collapseAdvancedMouseGestureSettings"></label>
    </div>
    <div id="advancedMouseGestureSettings" class="ol-collapsible hide">
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelDistanceThreshold" class="ol-name"></p>
          <p data-i18n="settingDescriptionDistanceThreshold" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.distanceThreshold" class="input-field" type="number" required step="1" max="999" min="0">
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p class="ol-name" data-i18n="settingLabelMatchingAlgorithm"></p>
          <p class="ol-description">
            <span data-i18n="settingDescriptionMatchingAlgorithm"></span>
            <a target="_blank" data-i18n="settingDescriptionMatchingAlgorithmLink" href="https://github.com/Robbendebiene/Gesturefy/wiki/Matching-Algorithm"></a>
          </p>
        </div>
        <div class="ol-input">
          <div class="select-wrapper">
            <select data-config="Settings.Gesture.matchingAlgorithm" class="select-field">
              <option value="combined" data-i18n="settingLabelMatchingAlgorithmCombined"></option>
              <option value="strict" data-i18n="settingLabelMatchingAlgorithmStrict"></option>
              <option value="shape-independent" data-i18n="settingLabelMatchingAlgorithmShapeIndependent"></option>
            </select>
          </div>
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelDeviationTolerance" class="ol-name"></p>
          <p data-i18n="settingDescriptionDeviationTolerance" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.deviationTolerance" class="input-field" type="number" required step="0.01" max="1" min="0">
        </div>
      </div>
    </div>
  </section>

  <section class="ol-section">
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="settingLabelDisplayTrace" class="ol-name"></p>
        <p data-i18n="settingDescriptionDisplayTrace" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <input data-collapse="#traceSettings" data-config="Settings.Gesture.Trace.display" class="toggle-button" type="checkbox">
      </div>
    </div>
    <div id="traceSettings" class="ol-collapsible">
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelTraceColor" class="ol-name"></p>
        </div>
        <div class="ol-input">
          <color-picker data-config="Settings.Gesture.Trace.Style.strokeStyle" class="color-select-field"></color-picker>
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelTraceGrowth" class="ol-name"></p>
          <p data-i18n="settingDescriptionTraceGrowth" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.Trace.Style.lineGrowth" class="toggle-button" type="checkbox">
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelTraceWidth" class="ol-name"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.Trace.Style.lineWidth" class="input-field" type="number" required step="1" max="999" min="1">
        </div>
      </div>
    </div>
  </section>

  <section class="ol-section">
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="settingLabelDisplayCommand" class="ol-name"></p>
        <p data-i18n="settingDescriptionDisplayCommand" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <input data-collapse="#commandSettings" data-config="Settings.Gesture.Command.display" class="toggle-button" type="checkbox">
      </div>
    </div>
    <div id="commandSettings" class="ol-collapsible">
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelCommandFontColor" class="ol-name"></p>
        </div>
        <div class="ol-input">
          <color-picker data-config="Settings.Gesture.Command.Style.fontColor" class="color-select-field"></color-picker>
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelCommandBackgroundColor" class="ol-name"></p>
        </div>
        <div class="ol-input">
          <color-picker data-config="Settings.Gesture.Command.Style.backgroundColor" class="color-select-field"></color-picker>
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelCommandFontSize" class="ol-name"></p>
          <p data-i18n="settingDescriptionCommandFontSize" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.Command.Style.fontSize" class="input-field" type="input" required pattern="[1-9][0-9]*(px|em|rem|ex|%|in|cm|mm|pt|pc|vh|vw|vmin|vmax)">
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelCommandHorizontalPosition" class="ol-name"></p>
          <p data-i18n="settingDescriptionCommandHorizontalPosition" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.Command.Style.horizontalPosition" class="input-field" type="number" min="0" max="100" required>
        </div>
      </div>
      <div class="ol-item">
        <div class="ol-label">
          <p data-i18n="settingLabelCommandVerticalPosition" class="ol-name"></p>
          <p data-i18n="settingDescriptionCommandVerticalPosition" class="ol-description"></p>
        </div>
        <div class="ol-input">
          <input data-config="Settings.Gesture.Command.Style.verticalPosition" class="input-field" type="number" min="0" max="100" required>
        </div>
      </div>
    </div>
  </section>

  <section class="ol-section">
    <div class="ol-item">
      <div class="ol-label">
        <p data-i18n="settingLabelUpdateNotification" class="ol-name"></p>
        <p data-i18n="settingDescriptionUpdateNotification" class="ol-description"></p>
      </div>
      <div class="ol-input">
        <input data-config="Settings.General.updateNotification" class="toggle-button" type="checkbox">
      </div>
    </div>
  </section>
</div>
