import { useState, useCallback, useRef } from "react";
import { useTranslation } from "react-i18next";
import type { SplitViewLayout } from "@/types/pref.ts";

interface SplitViewSandboxProps {
  onLayoutChange?: (layout: SplitViewLayout) => void;
  resizable?: boolean;
  className?: string;
}

const paneColors = [
  "bg-primary/20 border-primary/30",
  "bg-secondary/20 border-secondary/30",
  "bg-accent/20 border-accent/30",
  "bg-info/20 border-info/30",
];

const layouts: { value: SplitViewLayout; paneCount: number }[] = [
  { value: "horizontal", paneCount: 2 },
  { value: "vertical", paneCount: 2 },
  { value: "grid-2x2", paneCount: 4 },
  { value: "grid-3pane-left-main", paneCount: 3 },
  { value: "grid-3pane-right-main", paneCount: 3 },
  { value: "grid-3pane-top-main", paneCount: 3 },
  { value: "grid-3pane-bottom-main", paneCount: 3 },
];

function getLayoutGrid(layout: SplitViewLayout, paneCount: number) {
  const panes: { style: React.CSSProperties; idx: number }[] = [];

  switch (layout) {
    case "horizontal":
      for (let i = 0; i < paneCount; i++) {
        panes.push({ style: { flex: 1 }, idx: i });
      }
      return { panes, containerStyle: { display: "flex", flexDirection: "row" as const } };
    case "vertical":
      for (let i = 0; i < paneCount; i++) {
        panes.push({ style: { flex: 1 }, idx: i });
      }
      return { panes, containerStyle: { display: "flex", flexDirection: "column" as const } };
    case "grid-2x2":
      for (let i = 0; i < Math.min(paneCount, 4); i++) {
        panes.push({ style: {}, idx: i });
      }
      return { panes, containerStyle: { display: "grid", gridTemplateColumns: "1fr 1fr", gridTemplateRows: "1fr 1fr" } };
    case "grid-3pane-left-main":
      panes.push({ style: { gridColumn: "1", gridRow: "1 / 3" }, idx: 0 });
      for (let i = 1; i < paneCount; i++) {
        panes.push({ style: { gridColumn: "2" }, idx: i });
      }
      return { panes, containerStyle: { display: "grid", gridTemplateColumns: "1.5fr 1fr", gridTemplateRows: "1fr 1fr" } };
    case "grid-3pane-right-main":
      for (let i = 0; i < paneCount - 1; i++) {
        panes.push({ style: { gridColumn: "1" }, idx: i });
      }
      panes.push({ style: { gridColumn: "2", gridRow: "1 / 3" }, idx: paneCount - 1 });
      return { panes, containerStyle: { display: "grid", gridTemplateColumns: "1fr 1.5fr", gridTemplateRows: "1fr 1fr" } };
    case "grid-3pane-top-main":
      panes.push({ style: { gridRow: "1", gridColumn: "1 / 3" }, idx: 0 });
      for (let i = 1; i < paneCount; i++) {
        panes.push({ style: { gridColumn: "" }, idx: i });
      }
      return { panes, containerStyle: { display: "grid", gridTemplateColumns: "1fr 1fr", gridTemplateRows: "1.5fr 1fr" } };
    case "grid-3pane-bottom-main":
      for (let i = 0; i < paneCount - 1; i++) {
        panes.push({ style: { gridColumn: "" }, idx: i });
      }
      panes.push({ style: { gridRow: "2", gridColumn: "1 / 3" }, idx: paneCount - 1 });
      return { panes, containerStyle: { display: "grid", gridTemplateColumns: "1fr 1fr", gridTemplateRows: "1fr 1.5fr" } };
    default:
      for (let i = 0; i < paneCount; i++) {
        panes.push({ style: { flex: 1 }, idx: i });
      }
      return { panes, containerStyle: { display: "flex", flexDirection: "row" as const } };
  }
}

export function SplitViewSandbox({ onLayoutChange, resizable = false, className = "" }: SplitViewSandboxProps) {
  const { t } = useTranslation();
  const [layout, setLayout] = useState<SplitViewLayout>("horizontal");
  const [paneCount, setPaneCount] = useState(2);
  const [flexRatio, setFlexRatio] = useState(0.5);
  const resizing = useRef(false);
  const containerBoxRef = useRef<HTMLDivElement>(null);

  const handleLayoutChange = useCallback(
    (newLayout: SplitViewLayout) => {
      setLayout(newLayout);
      const layoutDef = layouts.find((l) => l.value === newLayout);
      if (layoutDef) {
        setPaneCount((prev) => Math.min(prev, layoutDef.paneCount));
      }
      onLayoutChange?.(newLayout);
    },
    [onLayoutChange],
  );

  const handleAddPane = useCallback(() => {
    setPaneCount((prev) => Math.min(prev + 1, 4));
  }, []);

  const handleRemovePane = useCallback(() => {
    setPaneCount((prev) => Math.max(prev - 1, 2));
  }, []);

  const { panes, containerStyle } = getLayoutGrid(layout, paneCount);

  const effectiveStyle = resizable && (layout === "horizontal" || layout === "vertical")
    ? {
        ...containerStyle,
        ...(layout === "horizontal"
          ? { gridTemplateColumns: `${flexRatio}fr ${1 - flexRatio}fr` }
          : { gridTemplateRows: `${flexRatio}fr ${1 - flexRatio}fr` }),
      }
    : containerStyle;

  const handleDividerDown = useCallback(
    (e: React.PointerEvent) => {
      e.preventDefault();
      resizing.current = true;
      const target = e.currentTarget;
      target.setPointerCapture(e.pointerId);

      const onMove = (ev: PointerEvent) => {
        if (!resizing.current || !containerBoxRef.current) return;
        const rect = containerBoxRef.current.getBoundingClientRect();
        if (layout === "horizontal") {
          const ratio = (ev.clientX - rect.left) / rect.width;
          setFlexRatio(Math.max(0.2, Math.min(0.8, ratio)));
        } else {
          const ratio = (ev.clientY - rect.top) / rect.height;
          setFlexRatio(Math.max(0.2, Math.min(0.8, ratio)));
        }
      };
      const onUp = () => {
        resizing.current = false;
        target.removeEventListener("pointermove", onMove);
        target.removeEventListener("pointerup", onUp);
      };
      target.addEventListener("pointermove", onMove);
      target.addEventListener("pointerup", onUp);
    },
    [layout],
  );

  return (
    <div className={`flex flex-col gap-3 ${className}`}>
      {/* Mock browser window */}
      <div className="border border-base-300 rounded-xl overflow-hidden" style={{ aspectRatio: "16/10" }}>
        {/* Title bar */}
        <div className="bg-base-300/60 px-3 py-1.5 flex items-center gap-1.5">
          <div className="flex gap-1">
            <div className="w-2.5 h-2.5 rounded-full bg-error/60" />
            <div className="w-2.5 h-2.5 rounded-full bg-warning/60" />
            <div className="w-2.5 h-2.5 rounded-full bg-success/60" />
          </div>
          <div className="flex-1 flex justify-center">
            <div className="bg-base-200/60 rounded px-3 py-0.5 text-[10px] text-base-content/40 w-1/2 text-center">
              floorp.app
            </div>
          </div>
        </div>

        {/* Tab bar */}
        <div className="bg-base-200/80 px-2 py-1 flex items-center gap-1 border-b border-base-300/60">
          {Array.from({ length: Math.min(paneCount + 1, 4) }).map((_, i) => (
            <div
              key={i}
              className={`px-2 py-0.5 rounded-t text-[9px] ${i === 0 ? "bg-base-100 text-base-content/70" : "text-base-content/30"}`}
            >
              Tab {i + 1}
            </div>
          ))}
          <div className="px-1 text-[10px] text-base-content/20">+</div>
        </div>

        {/* Content area with split */}
        <div
          ref={containerBoxRef}
          className="flex-1 bg-base-100 p-1.5 gap-1"
          style={{ ...effectiveStyle, height: "calc(100% - 52px)" }}
        >
          {panes.map((pane, i) => (
            <div
              key={i}
              className={`rounded border ${paneColors[pane.idx % paneColors.length]} p-2 flex flex-col gap-1 overflow-hidden`}
              style={pane.style}
            >
              <div className="w-3/4 h-2 bg-base-content/10 rounded" />
              <div className="w-1/2 h-2 bg-base-content/10 rounded" />
              <div className="w-2/3 h-2 bg-base-content/10 rounded" />
            </div>
          ))}
          {resizable && paneCount === 2 && (layout === "horizontal" || layout === "vertical") && (
            <div
              className="bg-base-content/10 hover:bg-primary/30 cursor-col-resize z-10 flex items-center justify-center"
              style={{
                ...(layout === "horizontal"
                  ? { width: 4, gridColumn: "2", gridRow: "1" }
                  : { height: 4, gridRow: "2", gridColumn: "1" }),
              }}
              onPointerDown={handleDividerDown}
            >
              <div
                className={`bg-base-content/20 rounded ${layout === "horizontal" ? "w-0.5 h-4" : "h-0.5 w-4"}`}
              />
            </div>
          )}
        </div>
      </div>

      {/* Controls */}
      <div className="flex flex-wrap items-center gap-2">
        <span className="text-xs text-base-content/60">{t("tutorial.splitviewSandbox.changeLayout")}:</span>
        {layouts.map((l) => (
          <button
            key={l.value}
            type="button"
            onClick={() => handleLayoutChange(l.value)}
            className={`btn btn-xs ${layout === l.value ? "btn-primary" : "btn-ghost"}`}
          >
            {t(`splitView.${layoutLabelKey(l.value)}`)}
          </button>
        ))}
      </div>
      <div className="flex items-center gap-2">
        <button
          type="button"
          onClick={handleRemovePane}
          disabled={paneCount <= 2}
          className="btn btn-xs btn-ghost"
        >
          {t("tutorial.splitviewSandbox.removePane")}
        </button>
        <span className="text-xs text-base-content/60">
          {paneCount} panes
        </span>
        <button
          type="button"
          onClick={handleAddPane}
          disabled={paneCount >= 4}
          className="btn btn-xs btn-ghost"
        >
          {t("tutorial.splitviewSandbox.addPane")}
        </button>
      </div>
    </div>
  );
}

function layoutLabelKey(layout: SplitViewLayout): string {
  const map: Record<SplitViewLayout, string> = {
    horizontal: "layoutHorizontal",
    vertical: "layoutVertical",
    "grid-2x2": "layoutGrid2x2",
    "grid-3pane-left-main": "layout3PaneLeft",
    "grid-3pane-right-main": "layout3PaneRight",
    "grid-3pane-top-main": "layout3PaneTop",
    "grid-3pane-bottom-main": "layout3PaneBottom",
  };
  return map[layout];
}
