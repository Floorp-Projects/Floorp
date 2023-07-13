// Missing file ./bad/module_1.mjs should be mapped to ./good/module_1.mjs.
// eslint-disable-next-line import/no-unassigned-import, import/no-unresolved
import {} from "./bad/module_1.mjs";
