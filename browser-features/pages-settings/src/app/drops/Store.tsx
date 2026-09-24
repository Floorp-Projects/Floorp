/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// 一枚の表。**中身を読む前に、まずここ。**
//
// 人が最初に知りたいのは「何をするものか」「誰が作ったか」で、コードはその次
// (読みたい人だけ)。だから表と裏に分けた:
//
//   表(ここ)  絵、説明、作った人、そして「入れる」
//   裏         中身を読む(権限 → この drop 自身の字 → 実行される file)
//
// 押してから入るまでは三段: 確かめる(sha256) → 何を許すのかを見せる → 許されたら入れる。
// 途中でやめられるし、やめても何も起きていない。ここに出ているものは全部、落として
// sha256 を確かめた xpi の中から読んだもので、まだ何も実行していない。

import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Button } from "@/components/common/button.tsx";
import {
  allAttested,
  type DropInspection,
  dropsRpc,
} from "@/lib/rpc/drops.ts";
import { DropIcon } from "@/app/drops/Shelf.tsx";

function contactHref(contact: string): string | null {
  if (/^https?:\/\//.test(contact)) {
    return contact;
  }
  if (contact.startsWith("gh/")) {
    return `https://github.com/${contact.slice(3)}`;
  }
  if (contact.startsWith("mail/")) {
    return `mailto:${contact.slice(5)}`;
  }
  if (contact.startsWith("social/")) {
    return `https://${contact.slice(7)}`;
  }
  return null;
}

function Mark({ ok }: { ok: boolean }) {
  const { t } = useTranslation();
  return (
    <span className={ok ? "text-green-600" : "text-red-600"}>
      {ok ? t("drops.signatureOk") : t("drops.signatureBad")}
    </span>
  );
}

export function Store(
  { seen, installedVersion, busy, onInstall, onRead, onRemove, onLookAgain }: {
    seen: DropInspection;
    installedVersion?: string;
    busy: boolean;
    onInstall: () => void | Promise<unknown>;
    onRead: () => void;
    onRemove?: () => void;
    onLookAgain: () => void | Promise<unknown>;
  },
) {
  const { t } = useTranslation();
  const ok = allAttested(seen);
  const entry = seen.entries[0];
  const version = entry?.version ?? "?";
  const have = installedVersion !== undefined;
  const update = have && installedVersion !== version;

  // 三段: 確かめる → 何を許すのかを見せる → 許されたら入れる
  const [step, setStep] = useState<null | "checking" | "ask" | "installing">(
    null,
  );
  const [check, setCheck] = useState<
    { ok: boolean; checked: number; bad: string[] } | null
  >(null);

  const press = async () => {
    setStep("checking");
    setCheck(null);
    try {
      setCheck(await dropsRpc.verifyDrop(seen));
    } catch (e) {
      setCheck({ ok: false, checked: 0, bad: [String((e as Error)?.message ?? e)] });
    }
    setStep("ask");
  };

  // 入れる一拍。速く終わっても、**わざと**この状態を少し見せる ── 押した、という
  // 手応えが無いと、画面がひとりでに変わったように見える。短いので待たされている感じは出ない
  const INSTALL_BEAT_MS = 600;
  const installWithBeat = async () => {
    setStep("installing");
    const started = Date.now();
    try {
      await onInstall();
    } finally {
      const rest = INSTALL_BEAT_MS - (Date.now() - started);
      if (rest > 0) {
        await new Promise((resolve) => setTimeout(resolve, rest));
      }
      setStep(null);
      setCheck(null);
    }
  };

  const chrome = entry?.chrome === true;
  const sandboxed = entry?.sandboxed === true;
  const grants = entry?.permissions ?? [];
  const ownFiles = entry?.sources.length ?? 0;
  const contacts = seen.manifest.contact ?? [];
  const source = seen.manifest.source;

  return (
    <div className="space-y-6">
      <div className="flex items-start gap-4">
        <DropIcon item={seen} size="lg" />
        <div className="min-w-0">
          <h2 className="text-2xl font-bold">{seen.name}</h2>
          <p className="mt-1 text-sm text-base-content/70">
            {seen.manifest.note}
          </p>
          <p className="mt-2 flex flex-wrap items-center gap-2 text-xs text-base-content/60">
            <code className="rounded bg-base-content/10 px-1.5 py-0.5">
              {version}
            </code>
            <span>· {seen.registry.name}</span>
            <Mark ok={ok} />
            {have && (
              <span className="rounded-full bg-primary/15 px-2 py-0.5 text-primary">
                {update
                  ? t("drops.installedOld", { version: installedVersion })
                  : t("drops.installedBadge")}
              </span>
            )}
          </p>
        </div>
      </div>

      <div className="flex flex-wrap items-center gap-3">
        {step === null && !have && (
          <Button
            variant={ok ? "primary" : "danger"}
            disabled={busy}
            onClick={press}
            className="h-11 px-6 text-base"
          >
            {ok ? t("drops.install") : t("drops.installAnyway")}
          </Button>
        )}
        {step === null && have && update && (
          <Button
            variant="primary"
            disabled={busy}
            onClick={press}
            className="h-11 px-6 text-base"
          >
            {t("drops.installNew")}
          </Button>
        )}
        {step === null && have && onRemove && (
          <Button
            variant="secondary"
            disabled={busy}
            onClick={onRemove}
            className={update ? undefined : "h-11 px-6 text-base"}
          >
            {t("drops.uninstall")}
          </Button>
        )}
        {step === "checking" && (
          <Button variant="primary" disabled className="h-11 px-6 text-base">
            {t("drops.checking")}
          </Button>
        )}
        {step === "installing" && (
          <Button variant="primary" disabled className="h-11 px-6 text-base">
            <span className="mr-2 inline-block h-4 w-4 animate-spin rounded-full border-2 border-current border-t-transparent" />
            {t("drops.installing")}
          </Button>
        )}
        {(step === null || step === "ask") && (
          <Button variant="light" onClick={onRead} disabled={busy}>
            {t("drops.read")}
          </Button>
        )}
      </div>

      {step === "ask" && (
        <div
          className={`space-y-4 rounded-lg border p-4 ${
            check?.ok
              ? "border-base-content/20"
              : "border-red-500/50 bg-red-500/5"
          }`}
        >
          <p className="text-sm">
            <b>{seen.name}</b> {t("drops.grantTitle")}
            <span className="mt-1 block text-base-content/70">
              {check?.ok
                ? t("drops.grantOk", { n: check?.checked ?? 0 })
                : t("drops.grantBad", { bad: check?.bad.join(", ") ?? "" })}
            </span>
          </p>
          <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-sm">
            <dt className="font-medium">{t("drops.grantWhere")}</dt>
            <dd className="text-base-content/80">
              {chrome ? t("drops.grantWhereChrome") : t("drops.grantWherePage")}
            </dd>
            <dt className="font-medium">{t("drops.grantCan")}</dt>
            <dd className="text-base-content/80">
              {sandboxed
                ? (
                  <ul className="list-disc space-y-1 pl-4">
                    {grants.map((g) => <li key={g.name}>{g.ja}</li>)}
                    {grants.length === 0 && <li>{t("drops.grantNothing")}</li>}
                    <li className="text-base-content/50">
                      {t("drops.grantOnly")}
                    </li>
                  </ul>
                )
                : (
                  <>
                    <b>{t("drops.grantOpen")}</b>{" "}
                    <button
                      type="button"
                      className="text-primary underline"
                      onClick={onRead}
                    >
                      {t("drops.grantReadFirst")}
                    </button>
                  </>
                )}
            </dd>
            <dt className="font-medium">{t("drops.grantUndo")}</dt>
            <dd className="text-base-content/80">{t("drops.grantUndoText")}</dd>
          </dl>
          <div className="flex flex-wrap gap-3">
            {check?.ok
              ? (
                <Button
                  variant={ok ? "primary" : "danger"}
                  disabled={busy}
                  onClick={() => void installWithBeat()}
                >
                  {t("drops.grantAllow")}
                </Button>
              )
              : (
                // 落としてあるものが揃っていない(戻したあとなど)。入れる前に、もう一度見る
                <Button
                  variant="primary"
                  disabled={busy}
                  onClick={() => void onLookAgain()}
                >
                  {t("drops.lookAgain")}
                </Button>
              )}
            <Button
              variant="ghost"
              disabled={busy}
              onClick={() => {
                setStep(null);
                setCheck(null);
              }}
            >
              {t("drops.grantCancel")}
            </Button>
          </div>
        </div>
      )}

      {!ok && (
        <p className="rounded-lg border border-red-500/40 bg-red-500/5 p-3 text-sm">
          <strong>{t("drops.cautionTitle")}</strong> {t("drops.cautionText")}
        </p>
      )}

      {seen.shots && seen.shots.length > 0 && (
        <div className="flex flex-wrap gap-3">
          {seen.shots.map((shot) => (
            <img
              key={shot.file}
              src={shot.dataUri}
              alt={shot.file}
              className="max-h-64 rounded-lg border border-base-content/20"
            />
          ))}
        </div>
      )}

      <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-sm">
        <dt className="font-medium">{t("drops.factsWhere")}</dt>
        <dd className="text-base-content/80">
          {chrome ? t("drops.factsWhereChrome") : t("drops.factsWherePage")}
        </dd>
        <dt className="font-medium">{t("drops.factsCan")}</dt>
        <dd className="text-base-content/80">
          {sandboxed
            ? t("drops.factsCanSandboxed", { n: grants.length })
            : t("drops.factsCanRaw")}
        </dd>
        <dt className="font-medium">{t("drops.factsRead")}</dt>
        <dd className="text-base-content/80">
          {t("drops.factsReadText", { n: ownFiles })}
        </dd>
        {contacts.length > 0 && (
          <>
            <dt className="font-medium">{t("drops.factsAuthor")}</dt>
            <dd className="flex flex-wrap gap-x-2 text-base-content/80">
              {contacts.map((contact) => {
                const href = contactHref(contact);
                return href
                  ? (
                    <a
                      key={contact}
                      href={href}
                      target="_blank"
                      rel="noreferrer"
                      className="text-primary underline"
                    >
                      {contact}
                    </a>
                  )
                  : <span key={contact}>{contact}</span>;
              })}
            </dd>
          </>
        )}
        {source?.repo && (
          <>
            <dt className="font-medium">{t("drops.factsSource")}</dt>
            <dd>
              <a
                href={`${source.repo}/tree/${source.commit ?? ""}`}
                target="_blank"
                rel="noreferrer"
                className="break-all text-primary underline"
              >
                {source.repo} @ {(source.commit ?? "").slice(0, 10)}
              </a>
            </dd>
          </>
        )}
      </dl>

      <p className="text-xs text-base-content/50">{t("drops.foot")}</p>
    </div>
  );
}
