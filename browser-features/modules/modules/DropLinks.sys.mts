// SPDX-License-Identifier: MPL-2.0
/**
 * DropLinks: registry の xpi へのリンクを、drops の「見る」へ。
 *
 * noraneko.f3liz.casa/drops/<uuid>/ の「install」は、dl.f3liz.casa/drop/<uuid>/<actor>.xpi への普通のリンク
 * (AMO の「Firefox に追加」と同じ絵)。Firefox はそれを application/x-xpinstall として amContentHandler に渡し、
 * add-on のインストール(署名が要る)を始めてしまう。ここでその口(nsIContentHandler の factory)を差し替えて、
 * URL が一覧の registry の <base>/<uuid>/….xpi なら、読み込みを止めて about:hub#/features/drops/<uuid> を開く。
 * 落とす・見る・入れる、は settings の側(本人が読んでから「入れる」)。ここでは何も落とさない。
 * registry のものでなければ、元の amContentHandler にそのまま渡す。
 */

const CONTRACT = "@mozilla.org/uriloader/content-handler;1?type=application/x-xpinstall";
const XPI_CONTENT_TYPE = "application/x-xpinstall";
const UUID = "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}";
// Floorp の gecko 型に NS_ERROR_WONT_HANDLE_CONTENT が無いので、実行時の Cr から取る
const WONT_HANDLE_CONTENT = (Cr as unknown as {
  NS_ERROR_WONT_HANDLE_CONTENT: number;
}).NS_ERROR_WONT_HANDLE_CONTENT;

/** ブラウザの窓の、リンクを開くところだけ */
interface DropLinkWindow {
  switchToTabHavingURI(url: string, openNewTab: boolean): void;
}

/** URL が一覧の registry の xpi なら { uuid, registry } */
export function matchDropUrl(spec: string): { uuid: string; registry: string } | null {
  const { listRegistries } = ChromeUtils.importESModule("resource://noraneko/modules/Drops.sys.mjs");
  for (const r of listRegistries()) {
    const base = r.base.replace(/\/$/, "");
    if (!spec.startsWith(base + "/")) continue;
    const m = spec.slice(base.length + 1).match(new RegExp(`^(${UUID})/[A-Za-z0-9._-]+\\.xpi(?:[?#].*)?$`));
    if (m) return { uuid: m[1], registry: r.name };
  }
  return null;
}

export function registerDropLinks(): void {
  const registrar = Components.manager.QueryInterface!(Ci.nsIComponentRegistrar);
  if (!registrar) {
    console.error("[noraneko-drops] nsIComponentRegistrar が取れない");
    return;
  }
  // 元の口(amContentHandler)は、差し替える前に一つ作って持っておく。registry のものでない xpi はこれに渡す
  const original = Cc[CONTRACT]?.createInstance(Ci.nsIContentHandler);
  if (!original) {
    console.error(`[noraneko-drops] ${CONTRACT} が無い`);
    return;
  }

  const handler = {
    handleContent(
      mimetype: string,
      context: nsIInterfaceRequestor,
      request: nsIRequest,
    ) {
      if (mimetype !== XPI_CONTENT_TYPE || !(request instanceof Ci.nsIChannel)) {
        throw Components.Exception("", WONT_HANDLE_CONTENT);
      }
      const channel = request as nsIChannel;
      const hit = matchDropUrl(channel.URI.spec);
      if (!hit) return original.handleContent(mimetype, context, request);

      request.cancel(Cr.NS_BINDING_ABORTED);
      const browser = channel.loadInfo.targetBrowsingContext?.top?.embedderElement as unknown as
        | { ownerGlobal?: DropLinkWindow }
        | undefined;
      const win = (browser?.ownerGlobal ??
        Services.wm.getMostRecentWindow("navigator:browser")) as
        | DropLinkWindow
        | null;
      const url = `about:hub#/features/drops/${hit.uuid}`;
      console.log(`[noraneko-drops] link ${channel.URI.spec} → ${url}`);
      win?.switchToTabHavingURI(url, true);
    },
    QueryInterface: ChromeUtils.generateQI(["nsIContentHandler"]),
  };
  registrar.registerFactory(
    Services.uuid.generateUUID(),
    "noraneko drop links (application/x-xpinstall)",
    CONTRACT,
    { createInstance: (iid: nsIID) => handler.QueryInterface(iid) } as nsIFactory,
  );
}
