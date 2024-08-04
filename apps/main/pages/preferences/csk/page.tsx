import { For } from "solid-js";
import {
  cskData,
  cskDatumToString,
  currentFocus,
  editingStatus,
  setCurrentFocus,
  setEditingStatus,
} from "./setkey";
import {
  commands,
  csk_category,
} from "@nora/shared/custom-shortcut-key/commands";

export function CustomShortcutKeyPage() {
  return (
    <>
      <div>
        <h1>カスタムショートカットキー</h1>
        <xul:description class="indent tip-caption">
          Floorp Daylight のキーボードショートカットをカスタマイズしましょう。
          Floorp Daylight には、80
          以上のカスタマイズ可能なキーボードショートカットが用意されています！重複したキーボードショートカットは機能しません。これらの設定を適用するには、
          Floorp Daylight を再起動してください。
        </xul:description>
        <xul:checkbox label="Firefox のキーボードショートカットを無効にする" />
      </div>
      <For each={csk_category}>
        {(category) => (
          <>
            <div
              data-l10n-id={"floorp-custom-actions-" + category}
              style={{
                "padding-top": "20px",
              }}
            >
              {category}
            </div>
            <For each={Object.entries(commands)}>
              {([key, value]) =>
                value.type === category ? (
                  <div style={{ display: "flex" }}>
                    <label
                      style={{ "flex-grow": "1" }}
                      data-l10n-id={
                        "floorp-custom-actions-" +
                        key.replace("floorp-", "").replace("gecko-", "")
                      }
                    >
                      {key}
                    </label>
                    <input
                      value={
                        currentFocus() === key && editingStatus() !== null
                          ? editingStatus()!
                          : cskDatumToString(cskData(), key)
                      }
                      onFocus={(ev) => {
                        setCurrentFocus(key);
                      }}
                      onBlur={(ev) => {
                        setEditingStatus(null);
                        if (currentFocus() === key) {
                          setCurrentFocus(null);
                        }
                      }}
                      readonly={true}
                      placeholder="Type a shortcut"
                      style={{
                        "border-radius": "5px",
                        border: "1px solid gray",
                        padding: "6px 10px",
                      }}
                    />
                  </div>
                ) : undefined
              }
            </For>
          </>
        )}
      </For>
    </>
  );
}
