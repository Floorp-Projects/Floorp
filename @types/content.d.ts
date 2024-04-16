import "global";
import "./gecko/lib.gecko.dom";

import type { JSServices } from "./gecko/lib.gecko.services";

declare global {
  const Services: JSServices;
}
