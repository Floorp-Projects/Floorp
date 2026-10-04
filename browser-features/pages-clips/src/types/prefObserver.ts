export type PrefValue = string | number | boolean | null;
export type PrefKind = "string" | "int" | "bool";

export interface ObservedPref {
  name: string;
  kind?: PrefKind;
}

export interface PrefReaders {
  getStringPref(name: string): Promise<string | null>;
  getIntPref(name: string): Promise<number | null>;
  getBoolPref(name: string): Promise<boolean | null>;
}
