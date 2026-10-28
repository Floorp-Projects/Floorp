// SPDX-License-Identifier: MPL-2.0
import type {
  DestinationPickerProps,
  BookmarkDialogProps,
  SubscribeDialogProps,
} from "./types.ts";
import { useEffect, useState } from "react";
import { ArrowUp, ChevronRight, Folder, LoaderCircle } from "lucide-react";
import { useTranslation } from "react-i18next";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import type { FolderView } from "../../../../../libs/newtab/types.ts";
import { messageOf, useNewTab } from "./state.tsx";
import { Button, IconButton, Modal, safeWebUrl } from "./ui.tsx";

function DestinationPicker({
  value,
  onChange,
  exclude,
  disabled,
}: DestinationPickerProps) {
  const { t } = useTranslation();
  const { snapshot } = useNewTab();
  const [view, setView] = useState<FolderView | null>(null);
  const [error, setError] = useState("");
  useEffect(() => {
    let active = true;
    if (value === snapshot.settings.speedDial.rootGuid) {
      setView(null);
      return;
    }
    void newTabClient
      .call("folder", { guid: value })
      .then((next) => {
        if (active) {
          setView(next);
          setError("");
        }
      })
      .catch((cause) => {
        if (active) setError(messageOf(cause));
      });
    return () => {
      active = false;
    };
  }, [value, snapshot.settings.speedDial.rootGuid]);
  const children =
    value === snapshot.settings.speedDial.rootGuid
      ? snapshot.groups
      : (view?.children.filter((node) => node.type === "folder") ?? []);
  const ancestors =
    view?.ancestors.filter(
      (node) =>
        node.guid !== snapshot.settings.speedDial.rootGuid &&
        node.guid !== view.guid,
    ) ?? [];
  const invalid =
    view?.guid === exclude ||
    view?.ancestors.some((node) => node.guid === exclude);
  return (
    <fieldset className="nt-destination" disabled={disabled}>
      <legend>{t("newtab.destination", "Save in")}</legend>
      <div className="nt-destination-groups">
        {snapshot.groups
          .filter((node) => node.guid !== exclude)
          .map((node) => (
            <button
              key={node.guid}
              type="button"
              onClick={() => onChange(node.guid)}
              aria-pressed={node.guid === value}
            >
              {node.title}
            </button>
          ))}
      </div>
      <div className="nt-destination-path">
        <IconButton
          label={t("newtab.goUp", "Go up one folder")}
          disabled={!view || !ancestors.length}
          onClick={() => onChange(ancestors.at(-1)!.guid)}
        >
          <ArrowUp size={17} />
        </IconButton>
        {ancestors.map((node) => (
          <span key={node.guid}>
            <button type="button" onClick={() => onChange(node.guid)}>
              {node.title}
            </button>
            <ChevronRight size={14} />
          </span>
        ))}
        <strong>
          {value === snapshot.settings.speedDial.rootGuid
            ? t("newtab.dial", "Speed Dial")
            : (view?.title ?? t("newtab.loading", "Loading…"))}
        </strong>
      </div>
      {error && <p role="alert">{error}</p>}
      {invalid && (
        <p role="alert">
          {t(
            "newtab.cycleError",
            "A folder cannot be moved into itself or its children.",
          )}
        </p>
      )}
      <div className="nt-destination-children">
        {!invalid &&
          children
            .filter((node) => node.guid !== exclude)
            .map((node) => (
              <button
                type="button"
                key={node.guid}
                onClick={() => onChange(node.guid)}
              >
                <Folder size={18} />
                {node.title}
                <ChevronRight size={15} />
              </button>
            ))}
      </div>
    </fieldset>
  );
}
function BookmarkDialog({ dialog, close }: BookmarkDialogProps) {
  const { t } = useTranslation();
  const { refresh, notify } = useNewTab();
  const [title, setTitle] = useState(dialog.node?.title ?? "");
  const [url, setUrl] = useState(dialog.node?.url ?? "");
  const [destination, setDestination] = useState(dialog.parentGuid);
  const [pending, setPending] = useState(false);
  const [error, setError] = useState("");
  const heading = dialog.node
    ? t("newtab.editBookmark", "Edit bookmark")
    : dialog.kind === "folder"
      ? t("newtab.createFolder", "Create folder")
      : t("newtab.addSite", "Add site");
  const submit = async (event: React.FormEvent) => {
    event.preventDefault();
    setPending(true);
    setError("");
    try {
      const normalized =
        dialog.kind === "bookmark"
          ? safeWebUrl(
              /^[a-z][a-z0-9+.-]*:/i.test(url.trim())
                ? url.trim()
                : `https://${url.trim()}`,
            )
          : null;
      if (
        dialog.kind === "bookmark" &&
        !normalized &&
        !(dialog.node && url.trim() === dialog.node.url)
      )
        throw new Error(
          t("newtab.invalidUrl", "Enter a valid HTTP or HTTPS URL."),
        );
      const name =
        title.trim() || (normalized ? new URL(normalized).hostname : "");
      if (!name)
        throw new Error(t("newtab.folderNameRequired", "Enter a folder name."));
      if (dialog.node) {
        if (dialog.node.parentGuid !== destination)
          await newTabClient.call("bookmark", {
            action: "move",
            guid: dialog.node.guid,
            parentGuid: destination,
          });
        await newTabClient.call("bookmark", {
          action: "update",
          guid: dialog.node.guid,
          title: name,
          ...(normalized ? { url: normalized } : {}),
        });
      } else
        await newTabClient.call("bookmark", {
          action: "create",
          type: dialog.kind,
          parentGuid: destination,
          title: name,
          ...(normalized ? { url: normalized } : {}),
        });
      await refresh();
      notify(t("newtab.saved", "Saved"));
      close();
    } catch (cause) {
      setError(messageOf(cause));
      await refresh();
    } finally {
      setPending(false);
    }
  };
  return (
    <Modal
      title={heading}
      closeLabel={t("newtab.close", "Close")}
      close={() => {
        if (!pending) close();
      }}
    >
      <form onSubmit={(event) => void submit(event)}>
        {dialog.kind === "bookmark" && (
          <label className="nt-dialog-field">
            <span>{t("newtab.url", "URL")}</span>
            <input
              value={url}
              onChange={(event) => setUrl(event.currentTarget.value)}
              required
              inputMode="url"
              autoComplete="url"
              placeholder="https://example.com"
              disabled={pending}
            />
          </label>
        )}
        <label className="nt-dialog-field">
          <span>
            {dialog.kind === "folder"
              ? t("newtab.folderName", "Folder name")
              : t("newtab.siteName", "Site name")}
          </span>
          <input
            value={title}
            onChange={(event) => setTitle(event.currentTarget.value)}
            maxLength={200}
            required={dialog.kind === "folder"}
            disabled={pending}
          />
        </label>
        <DestinationPicker
          value={destination}
          onChange={setDestination}
          exclude={dialog.kind === "folder" ? dialog.node?.guid : undefined}
          disabled={pending}
        />
        {error && (
          <p className="nt-form-error" role="alert">
            {error}
          </p>
        )}
        <div className="nt-dialog-actions">
          <Button
            variant="secondary"
            data-cancel
            disabled={pending}
            onClick={close}
          >
            {t("newtab.cancel", "Cancel")}
          </Button>
          <Button type="submit" disabled={pending}>
            {pending && <LoaderCircle size={17} className="nt-spin" />}
            {dialog.node ? t("newtab.save", "Save") : t("newtab.add", "Add")}
          </Button>
        </div>
      </form>
    </Modal>
  );
}
function SubscribeDialog({ close }: SubscribeDialogProps) {
  const { t } = useTranslation();
  const { refresh, notify } = useNewTab();
  const [url, setUrl] = useState("");
  const [pending, setPending] = useState(false);
  const [error, setError] = useState("");
  const [candidates, setCandidates] = useState<
    { title: string; url: string }[]
  >([]);
  const subscribe = async (value: string) => {
    setPending(true);
    setError("");
    try {
      const normalized = safeWebUrl(
        /^[a-z][a-z0-9+.-]*:/i.test(value.trim())
          ? value.trim()
          : `https://${value.trim()}`,
      );
      if (!normalized)
        throw new Error(
          t("newtab.invalidUrl", "Enter a valid HTTP or HTTPS URL."),
        );
      const result = await newTabClient.call("subscribe", { url: normalized });
      if (result.type === "discovery") {
        setCandidates(result.candidates);
        if (!result.candidates.length)
          setError(
            t(
              "newtab.noFeedFound",
              "No RSS or Atom feed was found on this page.",
            ),
          );
      } else {
        await refresh();
        notify(t("newtab.feedAdded", "Feed added"));
        close();
      }
    } catch (cause) {
      setError(messageOf(cause));
    } finally {
      setPending(false);
    }
  };
  return (
    <Modal
      title={t("newtab.addFeed", "Add feed")}
      closeLabel={t("newtab.close", "Close")}
      close={() => {
        if (!pending) close();
      }}
    >
      <form
        onSubmit={(event) => {
          event.preventDefault();
          void subscribe(url);
        }}
      >
        <label className="nt-dialog-field">
          <span>{t("newtab.feedUrl", "Feed or website URL")}</span>
          <input
            value={url}
            onChange={(event) => {
              setUrl(event.currentTarget.value);
              setCandidates([]);
            }}
            required
            inputMode="url"
            autoComplete="url"
            placeholder="https://example.com/feed.xml"
            disabled={pending}
          />
        </label>
        <p className="nt-dialog-hint">
          {t(
            "newtab.feedOnlyHint",
            "Only content supplied by the feed is displayed. Articles are not fetched from the original site.",
          )}
        </p>
        {candidates.length > 0 && (
          <fieldset className="nt-feed-candidates">
            <legend>{t("newtab.chooseFeed", "Choose a feed")}</legend>
            {candidates.map((candidate) => (
              <button
                type="button"
                key={candidate.url}
                disabled={pending}
                onClick={() => void subscribe(candidate.url)}
              >
                <strong>{candidate.title || candidate.url}</strong>
                <small>{candidate.url}</small>
              </button>
            ))}
          </fieldset>
        )}
        {error && (
          <p className="nt-form-error" role="alert">
            {error}
          </p>
        )}
        <div className="nt-dialog-actions">
          <Button
            variant="secondary"
            disabled={pending}
            data-cancel
            onClick={close}
          >
            {t("newtab.cancel", "Cancel")}
          </Button>
          <Button disabled={pending} type="submit">
            {pending && <LoaderCircle size={17} className="nt-spin" />}
            {t("newtab.add", "Add")}
          </Button>
        </div>
      </form>
    </Modal>
  );
}
export function Dialogs() {
  const { t } = useTranslation();
  const { dialog, setDialog, snapshot, refresh, notify } = useNewTab();
  const [pending, setPending] = useState(false);
  const [error, setError] = useState("");
  useEffect(() => {
    setError("");
    setPending(false);
  }, [dialog]);
  if (!dialog) return null;
  const close = () => {
    if (!pending) setDialog(null);
  };
  if (dialog.type === "bookmark")
    return (
      <BookmarkDialog
        key={dialog.node?.guid ?? `${dialog.kind}:${dialog.parentGuid}`}
        dialog={dialog}
        close={close}
      />
    );
  if (dialog.type === "subscribe") return <SubscribeDialog close={close} />;
  const confirm = async () => {
    setPending(true);
    setError("");
    try {
      if (dialog.type === "delete")
        await newTabClient.call("bookmark", {
          action: "delete",
          guid: dialog.summary.guid,
          expectedRevision: dialog.summary.revision,
        });
      if (dialog.type === "unsubscribe")
        await newTabClient.call("unsubscribe", { id: dialog.id });
      if (dialog.type === "block")
        await newTabClient.call("patchSettings", {
          expectedRevision: snapshot.settings.revision,
          changes: {
            topSites: {
              blocked: [
                ...new Set([...snapshot.settings.topSites.blocked, dialog.url]),
              ],
            },
          },
        });
      await refresh();
      notify(t("newtab.saved", "Saved"));
      setDialog(null);
    } catch (cause) {
      setError(messageOf(cause));
      if (dialog.type === "delete") {
        try {
          const latest = await newTabClient.call("deleteSummary", {
            guid: dialog.summary.guid,
          });
          if (latest.revision !== dialog.summary.revision) {
            setDialog({ ...dialog, summary: latest });
            notify(
              t(
                "newtab.contentsChanged",
                "The folder changed. Review the updated counts before deleting.",
              ),
            );
          }
        } catch {
          /* Keep the original failure visible; no deletion is inferred. */
        }
      }
    } finally {
      setPending(false);
    }
  };
  return (
    <Modal
      title={
        dialog.type === "unsubscribe"
          ? t("newtab.unsubscribe", "Unsubscribe")
          : dialog.type === "delete"
            ? t("newtab.delete", "Delete")
            : t("newtab.hide", "Hide")
      }
      closeLabel={t("newtab.close", "Close")}
      close={close}
    >
      <p className="nt-confirm-title">{dialog.title}</p>
      <p className="nt-dialog-hint">
        {dialog.type === "delete"
          ? t("newtab.deleteContents", {
              bookmarks: dialog.summary.bookmarks,
              folders: dialog.summary.folders,
              defaultValue:
                "This deletes {{bookmarks}} bookmarks and {{folders}} nested folders.",
            })
          : dialog.type === "unsubscribe"
            ? t(
                "newtab.unsubscribeHint",
                "Stop checking this feed and delete its unstarred cache? Starred articles will be kept.",
              )
            : t(
                "newtab.blockHint",
                "This site will no longer be suggested. You can restore it in Floorp Hub.",
              )}
      </p>
      {error && (
        <p className="nt-form-error" role="alert">
          {error}
        </p>
      )}
      <div className="nt-dialog-actions">
        <Button
          data-initial-focus
          data-cancel
          variant="secondary"
          disabled={pending}
          onClick={close}
        >
          {t("newtab.cancel", "Cancel")}
        </Button>
        <Button
          variant={dialog.type === "delete" ? "danger" : "primary"}
          disabled={pending || snapshot.readOnly}
          onClick={() => void confirm()}
        >
          {dialog.type === "delete"
            ? t("newtab.delete", "Delete")
            : dialog.type === "unsubscribe"
              ? t("newtab.unsubscribe", "Unsubscribe")
              : t("newtab.hide", "Hide")}
        </Button>
      </div>
    </Modal>
  );
}
