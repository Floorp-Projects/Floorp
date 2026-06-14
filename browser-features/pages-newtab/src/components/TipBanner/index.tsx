import { useCallback, useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { ExternalLink, Lightbulb, X } from "lucide-react";
import { rpc } from "../../lib/rpc/rpc.ts";

interface Tip {
  id: string;
  titleKey: string;
  descriptionKey: string;
  hubUrl: string;
}

const tips: Tip[] = [
  {
    id: "split-view",
    titleKey: "tipBanner.tips.splitView.title",
    descriptionKey: "tipBanner.tips.splitView.description",
    hubUrl: "about:hub#/features/splitview",
  },
  {
    id: "panel-sidebar",
    titleKey: "tipBanner.tips.panelSidebar.title",
    descriptionKey: "tipBanner.tips.panelSidebar.description",
    hubUrl: "about:hub#/features/sidebar",
  },
  {
    id: "workspaces",
    titleKey: "tipBanner.tips.workspaces.title",
    descriptionKey: "tipBanner.tips.workspaces.description",
    hubUrl: "about:hub#/features/workspaces",
  },
  {
    id: "mouse-gesture",
    titleKey: "tipBanner.tips.mouseGesture.title",
    descriptionKey: "tipBanner.tips.mouseGesture.description",
    hubUrl: "about:hub#/features/gesture",
  },
  {
    id: "keyboard-shortcuts",
    titleKey: "tipBanner.tips.shortcuts.title",
    descriptionKey: "tipBanner.tips.shortcuts.description",
    hubUrl: "about:hub#/features/shortcuts",
  },
];

const PREF_SEEN_TIPS = "floorp.newtab.tips.seen";
const PREF_DISMISSED_SESSION = "floorp.newtab.tips.sessionDismissed";

async function getSeenTips(): Promise<string[]> {
  try {
    const raw = await rpc.getStringPref(PREF_SEEN_TIPS);
    return raw ? JSON.parse(raw) : [];
  } catch {
    return [];
  }
}

async function markTipSeen(id: string): Promise<void> {
  try {
    const seen = await getSeenTips();
    if (!seen.includes(id)) {
      seen.push(id);
      await rpc.setStringPref(PREF_SEEN_TIPS, JSON.stringify(seen));
    }
  } catch (error) {
    console.error("[TipBanner] Failed to mark tip as seen", error);
  }
}

function getSessionDismissed(): boolean {
  try {
    return sessionStorage.getItem(PREF_DISMISSED_SESSION) === "true";
  } catch {
    return false;
  }
}

function setSessionDismissed(): void {
  try {
    sessionStorage.setItem(PREF_DISMISSED_SESSION, "true");
  } catch {
    // ignore
  }
}

export function TipBanner() {
  const { t } = useTranslation();
  const [currentTip, setCurrentTip] = useState<Tip | null>(null);
  const [dismissed, setDismissed] = useState(false);
  const [mounted, setMounted] = useState(false);
  const dismissTimerRef = useRef<ReturnType<typeof setTimeout>>(null);

  useEffect(() => {
    if (getSessionDismissed()) return;

    const pickTip = async () => {
      const seen = await getSeenTips();
      const unseen = tips.filter((tip) => !seen.includes(tip.id));
      const pool = unseen.length > 0 ? unseen : tips;
      const tip = pool[Math.floor(Math.random() * pool.length)];
      setCurrentTip(tip);
      requestAnimationFrame(() => setMounted(true));
    };

    pickTip();
  }, []);

  const handleDismiss = useCallback(() => {
    if (currentTip) {
      void markTipSeen(currentTip.id);
    }
    setMounted(false);
    setDismissed(true);
    setSessionDismissed();
    dismissTimerRef.current = setTimeout(() => setCurrentTip(null), 300);
  }, [currentTip]);

  const handleOpenHub = useCallback(() => {
    if (currentTip) {
      void markTipSeen(currentTip.id);
      globalThis.NRAddTab?.(currentTip.hubUrl);
    }
  }, [currentTip]);

  useEffect(() => {
    return () => {
      if (dismissTimerRef.current) clearTimeout(dismissTimerRef.current);
    };
  }, []);

  if (!currentTip || getSessionDismissed()) {
    return null;
  }

  const translateClass = dismissed
    ? "translate-y-4 lg:translate-y-0 lg:-translate-x-4 opacity-0"
    : mounted
    ? "translate-y-0 lg:translate-x-0 opacity-100"
    : "translate-y-4 lg:translate-y-0 lg:-translate-x-4 opacity-0";

  return (
    <div className="fixed bottom-4 left-4 right-4 lg:top-4 lg:bottom-auto z-40 flex justify-center lg:justify-start pointer-events-none">
      <div
        className={`pointer-events-auto max-w-lg w-full bg-base-100/90 backdrop-blur-sm border border-base-300 rounded-xl shadow-lg p-3 flex items-start gap-3 transition-all duration-300 ease-out ${translateClass}`}
      >
        <div className="text-warning flex-shrink-0 mt-0.5">
          <Lightbulb size={20} />
        </div>
        <div className="flex-1 min-w-0">
          <p className="font-medium text-sm">{t(currentTip.titleKey)}</p>
          <p className="text-xs text-base-content/70 mt-0.5">
            {t(currentTip.descriptionKey)}
          </p>
        </div>
        <div className="flex items-center gap-1 flex-shrink-0">
          <button
            type="button"
            onClick={handleOpenHub}
            className="btn btn-xs btn-ghost text-primary gap-1"
          >
            {t("tipBanner.learnMore")}
            <ExternalLink size={12} />
          </button>
          <button
            type="button"
            onClick={handleDismiss}
            className="btn btn-xs btn-ghost btn-circle"
            aria-label={t("tipBanner.dismiss")}
          >
            <X size={14} />
          </button>
        </div>
      </div>
    </div>
  );
}
