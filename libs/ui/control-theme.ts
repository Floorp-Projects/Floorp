import { createContext, useContext } from "react";

export const StandardControlsContext = createContext(false);
export const useStandardControls = () => useContext(StandardControlsContext);
