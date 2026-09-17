// SPDX-License-Identifier: MPL-2.0
export function usesSettingsActor(
  pageUrl: string,
  developmentPort: string,
): boolean {
  const url = new URL(pageUrl);
  return url.protocol === "http:" && url.port === developmentPort &&
    (url.hostname === "localhost" || url.hostname === "127.0.0.1");
}
