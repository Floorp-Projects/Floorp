import type { GestureAction, GestureDirection } from "@/types/pref.ts";

export interface ActionEditModalProps {
  action: GestureAction;
  mode: "new" | "edit" | "duplicate";
  onSave: (action: GestureAction) => Promise<boolean>;
  onClose: () => void;
  isPatternDuplicate: (pattern: GestureDirection[]) => boolean;
}
