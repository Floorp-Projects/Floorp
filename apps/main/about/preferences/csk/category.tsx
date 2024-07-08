/* @refresh reload */
import type {} from "@nora/solid-xul/jsx-runtime";

export function category() {
  return (
    <xul:richlistitem
      id="category-csk"
      class="category"
      value="paneCsk"
      helpTopic="prefs-csk"
      data-l10n-id="category-CSK"
      data-l10n-attrs="tooltiptext"
      align="center"
    >
      <xul:image class="category-icon" />
      <xul:label
        class="category-name"
        flex="1"
        data-l10n-id="category-CSK-title"
      >
        Custom Shortcut Keys
      </xul:label>
    </xul:richlistitem>
  );
}
