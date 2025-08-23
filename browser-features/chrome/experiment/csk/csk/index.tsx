import type {} from "solid-styled-jsx";
import { CustomShortcutKeyPage } from "./page";

import { initSetKey } from "./setkey";

export function csk() {
  initSetKey();
  return <CustomShortcutKeyPage />;
}
