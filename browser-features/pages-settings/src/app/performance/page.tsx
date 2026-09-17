/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import { useTranslation } from "react-i18next";
import { IdleMemoryReclaim } from "@/app/performance/components/IdleMemoryReclaim.tsx";

export default function Page() {
  const { t } = useTranslation();

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      <div className="floorp-page-header">
        <h1 className="floorp-page-heading">{t("performance.title")}</h1>
        <p className="floorp-page-description">{t("performance.description")}</p>
      </div>

      <div className={styles.sections}>
        <IdleMemoryReclaim />
      </div>
    </div>
  );
}
