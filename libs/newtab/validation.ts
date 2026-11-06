// SPDX-License-Identifier: MPL-2.0
import { isRecord } from "./origin.ts";
import type { NewTabMethod, NewTabRequest } from "./types.ts";
const METHODS = new Set<NewTabMethod>([
  "snapshot",
  "patchSettings",
  "folder",
  "deleteSummary",
  "bookmark",
  "feeds",
  "articles",
  "article",
  "subscribe",
  "unsubscribe",
  "refresh",
  "articleState",
  "articleImage",
  "dismissSponsor",
  "restoreSponsors",
  "saveSponsor",
  "focusUrlBar",
  "openHub",
  "history",
  "pickFolder",
  "folderImage",
  "repairBookmarks",
]);
function fail(): never {
  throw Object.assign(new Error("The browser request is invalid."), {
    code: "VALIDATION",
  });
}
function string(value: unknown, limit = 256): value is string {
  return typeof value === "string" && value.length > 0 && value.length <= limit;
}
function guid(value: unknown): value is string {
  return string(value, 12) && /^[A-Za-z0-9_-]{12}$/.test(value);
}
function ids(value: unknown): boolean {
  return Array.isArray(value) && value.length <= 1000 &&
    value.every((item) => string(item));
}
export function validateNewTabRequest(value: unknown): NewTabRequest {
  if (
    !isRecord(value) || !string(value.method) ||
    !METHODS.has(value.method as NewTabMethod) || !isRecord(value.params)
  ) return fail();
  const p = value.params;
  switch (value.method) {
    case "folder":
    case "deleteSummary":
      if (!guid(p.guid)) fail();
      break;
    case "patchSettings":
      if (!Number.isSafeInteger(p.expectedRevision) || !isRecord(p.changes)) {
        fail();
      }
      if (
        isRecord(p.changes.background) &&
        p.changes.background.customImage !== undefined &&
        p.changes.background.customImage !== null
      ) {
        const image = p.changes.background.customImage;
        if (
          typeof image !== "string" || image.length > 5_000_000 ||
          !/^data:image\/(png|jpeg|webp|gif|avif|bmp);base64,[a-zA-Z0-9+/=]+$/
            .test(image)
        ) fail();
      }
      break;
    case "bookmark":
      if (
        !["create", "update", "move", "reorder", "delete"].includes(
          String(p.action),
        )
      ) fail();
      break;
    case "article":
    case "unsubscribe":
    case "dismissSponsor":
      if (!string(p.id)) fail();
      break;
    case "articleImage":
      if (!string(p.articleId) || !string(p.imageId)) fail();
      break;
    case "subscribe":
      if (!string(p.url, 8192)) fail();
      break;
    case "refresh":
      if (p.ids !== undefined && !ids(p.ids)) fail();
      break;
    case "articleState":
      if (
        !ids(p.ids) || (p.isRead === undefined && p.starred === undefined) ||
        (p.isRead !== undefined && typeof p.isRead !== "boolean") ||
        (p.starred !== undefined && typeof p.starred !== "boolean")
      ) fail();
      break;
    case "saveSponsor":
      if (
        !string(p.id) || !guid(p.parentGuid) || !string(p.expectedTitle, 200) ||
        !string(p.expectedUrl, 8192)
      ) fail();
      break;
    case "articles":
      if (p.feedId !== undefined && !string(p.feedId)) fail();
      if (
        p.query !== undefined &&
        (typeof p.query !== "string" || p.query.length > 500)
      ) fail();
      if (
        p.filter !== undefined &&
        !["all", "unread", "starred"].includes(String(p.filter))
      ) fail();
      for (const key of ["offset", "limit"]) {
        if (
          p[key] !== undefined &&
          (typeof p[key] !== "number" || !Number.isSafeInteger(p[key]) ||
            p[key] < 0 || (key === "limit" && p[key] > 100))
        ) fail();
      }
      break;
  }
  return value as unknown as NewTabRequest;
}
