// Should be remapped to good/module_3.mjs.
import { exportedFunction } from "./bad/module_3.mjs";

if (exportedFunction()) {
  success("circular_dependency.mjs");
}
