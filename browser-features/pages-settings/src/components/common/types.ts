import type { SeekbarProps as LegacySeekbarProps } from "../../../../../libs/ui/types.ts";

export interface SeekbarProps extends Omit<LegacySeekbarProps, "onChange"> {
  onValueChange?: (value: number) => void;
}
