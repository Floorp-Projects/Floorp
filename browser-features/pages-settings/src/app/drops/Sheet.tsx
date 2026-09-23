/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// 一枚の裏 ── **中身を読むところ**。表(Store.tsx)に絵と説明と「入れる」があって、
// ここには読むためのものだけが並ぶ。ここに出ているものは全部「読んだだけ」で、
// まだ何も実行していない。

import { useTranslation } from "react-i18next";
import type {
  DropAttestation,
  DropEntryInspection,
  DropFile,
  DropInspection,
} from "@/lib/rpc/drops.ts";

function FileList(
  { files, title, note }: { files: DropFile[]; title: string; note?: string },
) {
  if (files.length === 0) {
    return null;
  }
  return (
    <div className="space-y-2">
      <h4 className="text-sm font-medium">{title}</h4>
      {note && <p className="text-xs text-base-content/50">{note}</p>}
      {files.map((file) => (
        <details key={file.path} className="rounded border border-base-content/15">
          <summary className="cursor-pointer px-3 py-2 font-mono text-sm">
            {file.path}
          </summary>
          <pre className="max-h-72 overflow-auto px-3 pb-3 text-xs break-all whitespace-pre-wrap">
            {file.text}
          </pre>
        </details>
      ))}
    </div>
  );
}

function Stamp(
  { attestation, registry }: { attestation: DropAttestation; registry: string },
) {
  const { t } = useTranslation();
  return (
    <p className="text-xs break-all">
      <span className={attestation.ok ? "text-green-600" : "text-red-600"}>
        {attestation.ok ? t("drops.signatureOk") : t("drops.signatureBad")}
      </span>{" "}
      <span className="text-base-content/60">
        {attestation.who} ({registry})
      </span>
      {attestation.rekorUrl && (
        <>
          {" · "}
          <a
            href={attestation.rekorUrl}
            target="_blank"
            rel="noreferrer"
            className="text-primary underline"
          >
            Rekor
          </a>
        </>
      )}
      {!attestation.ok && attestation.reason && (
        <span className="text-base-content/60"> — {attestation.reason}</span>
      )}
    </p>
  );
}

function Entry({ entry }: { entry: DropEntryInspection }) {
  const { t } = useTranslation();
  return (
    <div className="space-y-3 rounded-lg border border-base-content/20 p-4">
      <div className="flex flex-wrap items-center gap-2">
        <span className="font-medium">{entry.name}</span>
        <code className="rounded bg-base-content/10 px-1.5 py-0.5 text-xs">
          {entry.id} @ {entry.version}
        </code>
      </div>
      <p className="text-sm text-base-content/70">
        {t("drops.readRunsOn")}: {entry.matches.join(", ") || "—"}
      </p>
      {entry.chrome && (
        <p className="text-sm text-base-content/70">{t("drops.readChrome")}</p>
      )}
      <div>
        <h4 className="text-sm font-medium">{t("drops.permissions")}</h4>
        {entry.permissions.length === 0
          ? (
            <p className="text-sm text-base-content/60">
              {t("drops.noPermissions")}
            </p>
          )
          : (
            <ul className="list-disc pl-5 text-sm text-base-content/80">
              {entry.permissions.map((permission) => (
                <li key={permission.name}>{permission.ja}</li>
              ))}
            </ul>
          )}
      </div>
      {entry.functions.length > 0 && (
        <p className="text-sm text-base-content/70">
          {t("drops.grantFunctions")}: {entry.functions.join(", ")}
        </p>
      )}
      <FileList
        title={t("drops.readSource")}
        note={t("drops.readSourceNote")}
        files={entry.sources}
      />
      <FileList
        title={t("drops.readFiles")}
        note={t("drops.readFilesNote")}
        files={entry.files}
      />
    </div>
  );
}

export function Sheet({ seen }: { seen: DropInspection }) {
  const { t } = useTranslation();
  return (
    <div className="space-y-4">
      <div>
        <h3 className="text-lg font-medium">{t("drops.readTitle")}</h3>
        <p className="text-xs break-all text-base-content/50">
          {seen.uuid} · {seen.registry.name}
        </p>
      </div>
      {seen.attestations.length === 0
        ? (
          <p className="text-sm text-base-content/60">
            {t("drops.noSignature")}
          </p>
        )
        : (
          seen.attestations.map((attestation) => (
            <Stamp
              key={`${attestation.who}:${attestation.identity}`}
              attestation={attestation}
              registry={seen.registry.name}
            />
          ))
        )}
      {seen.entries.map((entry) => (
        <Entry key={entry.file} entry={entry} />
      ))}
      {seen.deps.length > 0 && (
        <p className="text-xs text-base-content/50">
          {t("drops.readDeps", { n: seen.deps.length })}
        </p>
      )}
    </div>
  );
}
