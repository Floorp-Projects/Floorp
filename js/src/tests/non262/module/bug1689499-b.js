// |reftest| skip -- support file

import A from "./bug1689499-a.js";
if (true) await 0;
export default "B";

