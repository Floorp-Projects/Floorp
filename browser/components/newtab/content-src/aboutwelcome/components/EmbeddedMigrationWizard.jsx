/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useRef } from "react";

export const EmbeddedMigrationWizard = ({ handleAction }) => {
  const ref = useRef();
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
      force-show-import-all="false"
      auto-request-state=""
      ref={ref}
    ></migration-wizard>
  );
};
