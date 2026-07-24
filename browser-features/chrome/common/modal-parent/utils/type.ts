export interface TFormItem {
  type:
    | "text"
    | "number"
    | "textarea"
    | "select"
    | "dropdown"
    | "checkbox"
    | "radio"
    | "url"
    /* Searchable grid over the bundled Material Symbols and Lucide sets.
       options are ignored; the picker supplies its own icons and hands
       back a self-contained data: URI. */
    | "icon-picker";
  id: string;
  label?: string;
  value?: string | number;
  required?: boolean;
  classList?: string;
  placeholder?: string;
  rows?: number;
  maxLength?: number;
  options?: Array<{
    label: string;
    value: string;
    icon?: string;
  }>;
  when?: {
    id: string;
    value: string | string[];
  };
  onInput?: (value: string) => string;
}

export interface TForm {
  forms: TFormItem[];
  title: string;
  submitLabel?: string;
  cancelLabel?: string;
}

export interface TFormResult {
  [key: string]: string | number;
}
