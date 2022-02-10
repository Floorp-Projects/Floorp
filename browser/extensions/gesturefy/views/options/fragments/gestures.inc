<div class="content-header">
  <h2 data-i18n="navigationGestures"></h2>
  <form id="gestureSearchForm" class="gl-search">
    <input id="gestureSearchInput" class="gl-search-input">
    <button id="gestureSearchToggleButton" class="gl-search-input-toggle" type="button"></button>
  </form>
</div>
<ul id="gestureContainer" class="gesture-list">
  <li>
    <button id="gestureAddButton" class="gl-add-button" type="button" data-i18n="gestureCreateNewButton"></button>
  </li>
</ul>

<popup-box id="gesturePopup">
  <span data-i18n="gesturePopupTitleNewGesture" id="gesturePopupHeading" slot="title"></span>
  <div class="gp-wrapper" slot="content">
    <div id="gesturePopupRecordingArea">
      <canvas id="gesturePopupCanvas"></canvas>
      <div id="gesturePopupPatternContainer"></div>
    </div>
    <form id="gesturePopupForm">
      <div class="gp-field">
        <span data-i18n="gesturePopupLabelCommand" class="gp-field-name"></span>
        <p data-i18n="gesturePopupDescriptionCommand" class="gp-field-description"></p>
        <command-select id="gesturePopupCommandSelect" class="command-select-field"></command-select>
      </div>
      <label class="gp-field">
        <span data-i18n="gesturePopupLabelOptionalLabel" class="gp-field-name"></span>
        <p data-i18n="gesturePopupDescriptionOptionalLabel" class="gp-field-description"></p>
        <input id="gesturePopupLabelInput" class="input-field" maxlength="100">
      </label>
      <button id="gesturePopupSaveButton" data-i18n="buttonSave" type="submit"></button>
    </form>
  </div>
</popup-box>
