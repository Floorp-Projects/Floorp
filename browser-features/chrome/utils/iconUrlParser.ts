import { devHttpOrigin } from "#libs/shared/dev-ports/defines.ts";
export function iconUrlParser(url: string): string {
  if (url.startsWith("data:")) {
    return url;
  }
  if (import.meta.env.DEV) {
    return devHttpOrigin("loader") + url;
  }
  return url;
}
