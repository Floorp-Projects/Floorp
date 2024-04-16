//https://github.com/nyanrus/XPIDL2DTS/blob/c6f33a55dd67ddf7cd7d2326bcabff3cb4509fdc/src/post-processing/gen_components.ts#L163C1-L169C2
import "./gecko/lib.gecko.services";

declare global {
  const Components: nsIXPCComponents;
  const Cc: nsIXPCComponents_Classes;
  const Cu: nsIXPCComponents_Utils;
  const Ci: nsIXPCComponents_Interfaces;
  //const Services: Services;
}
