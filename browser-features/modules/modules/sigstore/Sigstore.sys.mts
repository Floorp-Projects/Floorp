// SPDX-License-Identifier: MPL-2.0
/**
 * sigstore の keyless bundle をブラウザの中で確かめる。
 *
 * 中身は @freedomofpress/sigstore-browser(WebCrypto だけで動く verifier。sigstore-conformance を通している)。
 * trust root は sigstore/root-signing の targets/trusted_root.json を pin(trusted_root.json)。
 * TUF での更新は使わない(registry が trusted_root.json を更新して配る)。
 *
 * drops では、manifest.json への判(作者の workflow と registry の workflow)を確かめて、画面に「揃っているか」を出す。
 * 判は門番ではない。入れるかは本人が決める。
 */
import {
  SigstoreVerifier,
  type TrustedRoot,
} from "@freedomofpress/sigstore-browser";
import trustedRoot from "./trusted_root.json" with { type: "json" };

export interface AttestationCheck {
  who: string;
  identity: string;
  issuer: string;
  ok: boolean;
  reason?: string;
  logIndex?: number;
  rekorUrl?: string;
}

let verifierPromise: Promise<SigstoreVerifier> | null = null;
function verifier(): Promise<SigstoreVerifier> {
  if (!verifierPromise) {
    verifierPromise = (async () => {
      const v = new SigstoreVerifier();
      await v.loadSigstoreRoot(trustedRoot as unknown as TrustedRoot);
      return v;
    })();
  }
  return verifierPromise;
}

/** bundle(JSON)が、この identity / issuer の判として artifact に対して正しいか */
export async function verifyKeylessBundle(
  who: string,
  bundle: unknown,
  artifact: Uint8Array,
  identity: string,
  issuer: string,
): Promise<AttestationCheck> {
  const b = bundle as { verificationMaterial?: { tlogEntries?: { logIndex?: string | number }[] } };
  const logIndexRaw = b?.verificationMaterial?.tlogEntries?.[0]?.logIndex;
  const logIndex = logIndexRaw === undefined ? undefined : Number(logIndexRaw);
  const base = { who, identity, issuer, logIndex, rekorUrl: logIndex ? `https://search.sigstore.dev/?logIndex=${logIndex}` : undefined };
  try {
    const v = await verifier();
    const ok = await v.verifyArtifact(identity, issuer, bundle as never, artifact, false);
    return { ...base, ok, reason: ok ? undefined : "verifier returned false" };
  } catch (e) {
    return { ...base, ok: false, reason: String((e as Error)?.message ?? e) };
  }
}

/** 起動の自己確認(dev): library が bundle されて WebCrypto が使えるか。trust root の読み込みまで */
export async function selfCheck(): Promise<string> {
  const v = await verifier();
  return v ? "sigstore verifier ready (trust root loaded)" : "sigstore verifier missing";
}
