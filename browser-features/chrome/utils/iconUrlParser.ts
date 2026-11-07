import { launchDevPorts } from "../../modules/common/dev-port-offset.ts";

/** Launch-local dev port resolver (FDH Gate 2 task 2-B). */
const launchDevPort = launchDevPorts();
export function iconUrlParser(url: string): string {
  if (url.startsWith("data:")) {
    return url;
  }
  if (import.meta.env.DEV) {
    return launchDevPort.httpOrigin("loader") + url;
  }
  return url;
}
