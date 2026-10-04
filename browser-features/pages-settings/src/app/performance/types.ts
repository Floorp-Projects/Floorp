export interface NumberFieldProps {
  id: string;
  label: string;
  value: number;
  min: number;
  disabled: boolean;
  onCommit: (value: number) => void;
}
