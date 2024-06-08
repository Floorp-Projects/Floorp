/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useRef } from "react";

export const EmbeddedMigrationWizard = ({ handleAction, content }) => {
  const ref = useRef();
  const options = content.migration_wizard_options;
  useEffect(() => {
    const handleBeginMigration = () => {
      handleAction({
        currentTarget: { value: "migrate_start" },
        source: "primary_button",
      });
    };
    const handleClose = () => {
      handleAction({ currentTarget: { value: "migrate_close" } });
    };
    const { current } = ref;
    current?.addEventListener(
      "MigrationWizard:BeginMigration",
      handleBeginMigration
    );
    current?.addEventListener("MigrationWizard:Close", handleClose);
    return () => {
      current?.removeEventListener(
        "MigrationWizard:BeginMigration",
        handleBeginMigration
      );
      current?.removeEventListener("MigrationWizard:Close", handleClose);
    };
  }, []); // eslint-disable-line react-hooks/exhaustive-deps
  return (
    <migration-wizard
      in-aboutwelcome-bundle=""
      force-show-import-all={options?.force_show_import_all || "false"}
      auto-request-state=""
      ref={ref}
      option-expander-title-string={options?.option_expander_title_string || ""}
      hide-option-expander-subtitle={
        options?.hide_option_expander_subtitle || false
      }
      data-import-complete-success-string={
        options?.data_import_complete_success_string || ""
      }
      selection-header-string={options?.selection_header_string}
      selection-subheader-string={options?.selection_subheader_string || ""}
      hide-select-all={options?.hide_select_all || false}
      checkbox-margin-inline={options?.checkbox_margin_inline || ""}
      checkbox-margin-block={options?.checkbox_margin_block || ""}
      import-button-string={options?.import_button_string || ""}
      import-button-class={options?.import_button_class || ""}
      header-font-size={options?.header_font_size || ""}
      header-font-weight={options?.header_font_weight || ""}
      header-margin-block={options?.header_margin_block || ""}
      subheader-font-size={options?.subheader_font_size || ""}
      subheader-font-weight={options?.subheader_font_weight || ""}
      subheader-margin-block={options?.subheader_margin_block || ""}
    ></migration-wizard>
  );
};
