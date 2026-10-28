// SPDX-License-Identifier: MPL-2.0
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
) as {
  setTimeout: (callback: () => void, milliseconds: number) => number;
  clearTimeout: (timer: number) => void;
};
import { normaliseFeedUrl } from "./FeedParser.sys.mts";
import type { FeedResponse } from "./types.ts";

export const MAX_FEED_BYTES = 5 * 1024 * 1024;
export const MAX_IMAGE_BYTES = 8 * 1024 * 1024;
const REQUEST_TIMEOUT = 20_000;
const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs",
) as {
  NetUtil: {
    newChannel(
      options: {
        uri: string;
        loadingPrincipal: nsIPrincipal;
        securityFlags: number;
        contentPolicyType: number;
      },
    ): nsIChannel;
  };
};

/** Anonymous, bounded Gecko channels retain browser proxy/certificate behavior.
 * Each HTTP hop uses its own ordinary content principal. Native redirects are
 * stopped and reopened after validation, so opaque-response filtering cannot
 * hide cross-origin feed bytes and chrome authority is never lent to a URI. */
export function requestFeed(
  inputUrl: string,
  headers: Record<string, string> = {},
  signal?: AbortSignal,
  maxBytes = MAX_FEED_BYTES,
): Promise<FeedResponse> {
  const url = normaliseFeedUrl(inputUrl);
  return requestFeedHop(
    url,
    headers,
    signal,
    maxBytes,
    5,
    Date.now() + REQUEST_TIMEOUT,
  );
}

function requestFeedHop(
  url: string,
  headers: Record<string, string>,
  signal: AbortSignal | undefined,
  maxBytes: number,
  redirectsRemaining: number,
  deadline: number,
): Promise<FeedResponse> {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {
      reject(new Error("The feed request was cancelled."));
      return;
    }
    if (deadline <= Date.now()) {
      reject(new Error("The feed request timed out."));
      return;
    }
    const channel = NetUtil.newChannel({
      uri: url,
      loadingPrincipal: Services.scriptSecurityManager.createContentPrincipal(
        Services.io.newURI(url),
        {},
      ),
      securityFlags:
        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL! |
        Ci.nsILoadInfo.SEC_COOKIES_OMIT!,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_FETCH,
    }).QueryInterface!(Ci.nsIHttpChannel);
    channel.loadFlags |= Ci.nsIRequest.LOAD_ANONYMOUS!;
    channel.requestMethod = "GET";
    channel.setRequestHeader(
      "Accept",
      maxBytes === MAX_FEED_BYTES
        ? "application/rss+xml, application/atom+xml, application/xml, text/xml, text/html;q=0.8"
        : "image/avif,image/webp,image/png,image/jpeg,image/gif",
      false,
    );
    for (const [name, value] of Object.entries(headers)) {
      channel.setRequestHeader(name, value, false);
    }
    let active: nsIChannel = channel;
    let redirectUrl: string | null = null;
    let failure: Error | null = null;
    let size = 0;
    let settled = false;
    const chunks: Uint8Array[] = [];
    const abort = () => {
      failure ??= new Error("The feed request was cancelled.");
      active.cancel(Cr.NS_BINDING_ABORTED);
    };
    const timeout = setTimeout(() => {
      failure = new Error("The feed request timed out.");
      active.cancel(Cr.NS_ERROR_NET_TIMEOUT);
    }, Math.max(1, deadline - Date.now()));
    signal?.addEventListener("abort", abort, { once: true });
    const finish = () => {
      clearTimeout(timeout);
      signal?.removeEventListener("abort", abort);
    };
    const callbacks = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIInterfaceRequestor",
        "nsIChannelEventSink",
      ]),
      getInterface<T extends nsIID>(iid: T): nsQIResult<T> {
        return this.QueryInterface(iid) as nsQIResult<T>;
      },
      asyncOnChannelRedirect(
        _old: nsIChannel,
        next: nsIChannel,
        _flags: number,
        callback: nsIAsyncVerifyRedirectCallback,
      ) {
        try {
          const nextUrl = normaliseFeedUrl(next.URI.spec);
          if (redirectsRemaining <= 0) {
            throw new Error("The feed has too many redirects.");
          }
          redirectUrl = nextUrl;
          // We deliberately do not open the native cross-origin channel: its
          // original principal would turn the readable feed into an opaque body.
          callback.onRedirectVerifyCallback(Cr.NS_ERROR_ABORT);
        } catch (cause) {
          failure = cause instanceof Error
            ? cause
            : new Error("The feed redirected to an unsupported URL.");
          callback.onRedirectVerifyCallback(Cr.NS_ERROR_ABORT);
        }
      },
    };
    channel.notificationCallbacks = callbacks;
    const listener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIStreamListener",
        "nsIRequestObserver",
      ]),
      onStartRequest(request: nsIRequest) {
        active = request.QueryInterface!(Ci.nsIChannel);
      },
      onDataAvailable(
        request: nsIRequest,
        input: nsIInputStream,
        offsetOrCount: number,
        count?: number,
      ) {
        // Accept both listener layouts while generated IDL types and runtime
        // versions may differ. Invalid counts must never become an empty success.
        const amount = count ?? offsetOrCount;
        if (!Number.isSafeInteger(amount) || amount < 0) {
          failure = new Error(
            "The feed stream returned an invalid byte count.",
          );
          request.cancel(Cr.NS_ERROR_UNEXPECTED);
          return;
        }
        size += amount;
        if (size > maxBytes) {
          failure = new Error("The response exceeds the size limit.");
          request.cancel(Cr.NS_ERROR_FILE_TOO_BIG);
          return;
        }
        const stream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
          Ci.nsIBinaryInputStream,
        );
        stream.setInputStream(input);
        chunks.push(Uint8Array.from(stream.readByteArray(amount)));
      },
      onStopRequest(request: nsIRequest, status: nsresult) {
        if (settled) return;
        settled = true;
        finish();
        if (redirectUrl && !failure && !signal?.aborted) {
          const forwardedHeaders =
            new URL(redirectUrl).origin === new URL(url).origin ? headers : {};
          void requestFeedHop(
            redirectUrl,
            forwardedHeaders,
            signal,
            maxBytes,
            redirectsRemaining - 1,
            deadline,
          ).then(resolve, reject);
          return;
        }
        if (failure || !Components.isSuccessCode(status)) {
          reject(failure ?? new Error("The feed could not be downloaded."));
          return;
        }
        try {
          const http = request.QueryInterface!(Ci.nsIHttpChannel);
          const responseHeaders: Record<string, string> = {};
          http.visitResponseHeaders({
            visitHeader(name: string, value: string) {
              responseHeaders[name.toLowerCase()] = value;
            },
          });
          const bytes = new Uint8Array(size);
          let offset = 0;
          for (const chunk of chunks) {
            bytes.set(chunk, offset);
            offset += chunk.length;
          }
          resolve({
            status: http.responseStatus,
            url: http.URI.spec,
            bytes,
            headers: responseHeaders,
            contentType: http.contentType,
            charset: http.contentCharset,
          });
        } catch {
          reject(new Error("The feed response could not be read."));
        }
      },
    };
    try {
      channel.asyncOpen(listener);
    } catch {
      settled = true;
      finish();
      reject(new Error("The feed request could not be started."));
    }
  });
}

export function decodeFeedResponse(response: FeedResponse): string {
  let encoding = response.charset;
  if (!encoding) {
    const bytes = response.bytes;
    if (
      bytes[0] === 0xff && bytes[1] === 0xfe ||
      bytes[0] === 0x3c && bytes[1] === 0 && bytes[2] === 0x3f && bytes[3] === 0
    ) encoding = "utf-16le";
    else if (
      bytes[0] === 0xfe && bytes[1] === 0xff ||
      bytes[0] === 0 && bytes[1] === 0x3c && bytes[2] === 0 && bytes[3] === 0x3f
    ) encoding = "utf-16be";
    else {
      const beginning = new TextDecoder("windows-1252").decode(
        bytes.subarray(0, 1024),
      );
      encoding =
        beginning.match(/<\?xml[^>]*encoding\s*=\s*["']([^"']+)/i)?.[1] ??
          beginning.match(/<meta\b[^>]*charset\s*=\s*["']?([^\s"'/>;]+)/i)
            ?.[1] ??
          "utf-8";
    }
  }
  try {
    return new TextDecoder(encoding).decode(response.bytes);
  } catch {
    throw new Error("The feed uses an unsupported text encoding.");
  }
}
