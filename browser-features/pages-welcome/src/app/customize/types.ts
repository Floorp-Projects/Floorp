export interface SearchEngine {
  name: string;
  iconURL: string | null;
  identifier: string;
  id: string;
  telemetryId?: string;
  description?: string;
  hidden?: boolean;
  alias?: string;
  aliases?: string[];
  domain?: string;
  isAppProvided: boolean;
  isDefault?: boolean;
  isPrivateDefault?: boolean;
}

declare global {
  interface Window {
    NRGetSearchEngines(callback: (data: string) => void): void;
    NRGetDefaultEngine(callback: (data: string) => void): void;
    NRSetDefaultEngine(
      engineId: string,
      callback: (data: string) => void,
    ): void;
    NRGetDefaultPrivateEngine(callback: (data: string) => void): void;
    NRSetDefaultPrivateEngine(
      engineId: string,
      callback: (data: string) => void,
    ): void;
  }
}
