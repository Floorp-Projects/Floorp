// SPDX-License-Identifier: MPL-2.0

import {
  commands,
  csk_category,
} from "@nora/shared/custom-shortcut-key/commands";
import {
  cskData,
  cskDatumToString,
  currentFocus,
  editingStatus,
  setCurrentFocus,
  setEditingStatus,
} from "./setkey";

export function CustomShortcutKeyPage() {
  return (
    <>
      <xul:hbox
        id="cskCategory"
        class=""
        hidden="true"
        style="flex-direction: column"
        data-category="paneCSK"
      >
        <h1>カスタムショートカットキー</h1>
        <xul:description class="indent tip-caption">
          Floorp Daylight のキーボードショートカットをカスタマイズしましょう。
          Floorp Daylight には、80
          以上のカスタマイズ可能なキーボードショートカットが用意されています！重複したキーボードショートカットは機能しません。これらの設定を適用するには、
          Floorp Daylight を再起動してください。
        </xul:description>
        <xul:checkbox label="Firefox のキーボードショートカットを無効にする" />
        {csk_category.map((category) => (
          <>
            <div
              data-l10n-id={"floorp-custom-actions-" + category}
              style={{
                paddingTop: "20px",
              }}
            >
              {category}
            </div>
            {Object.entries(commands).map(([key, value]) =>
              value.type === category ? (
                <div style={{ display: "flex", paddingTop: "5px" }}>
                  <label
                    style={{ flexGrow: "1" }}
                    data-l10n-id={
                      "floorp-custom-actions-" +
                      key.replace("floorp-", "").replace("gecko-", "")
                    }
                  >
                    {key}
                  </label>
                  <input
                    value={
                      currentFocus.value === key && editingStatus.value !== null
                        ? editingStatus.value!
                        : cskDatumToString(cskData.value, key)
                    }
                    onFocus={(_ev) => {
                      setCurrentFocus(key);
                    }}
                    onBlur={(_ev) => {
                      setEditingStatus(null);
                      if (currentFocus.value === key) {
                        setCurrentFocus(null);
                      }
                    }}
                    readOnly
                    placeholder="Type a shortcut"
                    style={{
                      borderRadius: "5px",
                      border: "1px solid gray",
                      padding: "6px 10px",
                    }}
                  />
                </div>
              ) : null
            )}
          </>
        ))}
      </xul:hbox>
    </>
  );
}
