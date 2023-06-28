// Should be remapped to good/module_3.js.
import { exportedFunction } from "./bad/module_3.js";

if (exportedFunction()) {
  success("circular_dependency.js");
}
