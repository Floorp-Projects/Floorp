import { useState, useRef, useCallback, useEffect } from "react";
import { useTranslation } from "react-i18next";
import type { GestureDirection } from "@/types/pref.ts";

interface GestureCanvasProps {
  onGestureComplete: (directions: GestureDirection[]) => void;
  targetPattern?: GestureDirection[];
  trailColor?: string;
  trailWidth?: number;
  className?: string;
  actionMap?: Record<string, string>;
}

const directionArrows: Record<GestureDirection, string> = {
  up: "↑",
  down: "↓",
  left: "←",
  right: "→",
  upRight: "↗",
  upLeft: "↖",
  downRight: "↘",
  downLeft: "↙",
};

function detectDirection(dx: number, dy: number, threshold: number): GestureDirection | null {
  const absDx = Math.abs(dx);
  const absDy = Math.abs(dy);

  if (absDx < threshold && absDy < threshold) return null;

  const isDiagonal = absDx >= threshold * 0.6 && absDy >= threshold * 0.6;

  if (isDiagonal) {
    if (dx > 0 && dy < 0) return "upRight";
    if (dx < 0 && dy < 0) return "upLeft";
    if (dx > 0 && dy > 0) return "downRight";
    if (dx < 0 && dy > 0) return "downLeft";
  }

  if (absDx > absDy) {
    return dx > 0 ? "right" : "left";
  }
  return dy > 0 ? "down" : "up";
}

export function GestureCanvas({
  onGestureComplete,
  targetPattern,
  trailColor = "#4F46E5",
  trailWidth = 3,
  className = "",
  actionMap,
}: GestureCanvasProps) {
  const { t } = useTranslation();
  const [points, setPoints] = useState<{ x: number; y: number }[]>([]);
  const [directions, setDirections] = useState<GestureDirection[]>([]);
  const [isDrawing, setIsDrawing] = useState(false);
  const [matched, setMatched] = useState(false);
  const lastDirRef = useRef<GestureDirection | null>(null);
  const lastPointRef = useRef<{ x: number; y: number } | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const pointerIdRef = useRef<number | null>(null);

  const completeGesture = useCallback(
    (dirs: GestureDirection[]) => {
      if (dirs.length > 0) {
        onGestureComplete(dirs);
        if (targetPattern && dirs.length === targetPattern.length) {
          const isMatch = dirs.every((d, i) => d === targetPattern[i]);
          if (isMatch) setMatched(true);
        }
      }
    },
    [onGestureComplete, targetPattern],
  );

  useEffect(() => {
    if (!isDrawing) return;
    const el = containerRef.current;
    if (!el || pointerIdRef.current === null) return;

    const onMove = (e: PointerEvent) => {
      const rect = el.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      setPoints((prev) => [...prev, { x, y }]);
      if (!lastPointRef.current) return;
      const dx = x - lastPointRef.current.x;
      const dy = y - lastPointRef.current.y;
      const dir = detectDirection(dx, dy, 30);
      if (dir && dir !== lastDirRef.current) {
        lastDirRef.current = dir;
        lastPointRef.current = { x, y };
        setDirections((prev) => [...prev, dir]);
      }
    };
    const onUp = () => {
      setIsDrawing(false);
      setDirections((prev) => {
        completeGesture(prev);
        return prev;
      });
    };

    try { el.setPointerCapture(pointerIdRef.current!); } catch { /* ignore */ }
    el.addEventListener("pointermove", onMove);
    el.addEventListener("pointerup", onUp);
    return () => {
      try { el.releasePointerCapture(pointerIdRef.current!); } catch { /* ignore */ }
      el.removeEventListener("pointermove", onMove);
      el.removeEventListener("pointerup", onUp);
    };
  }, [isDrawing, completeGesture]);

  const handlePointerDown = useCallback((e: React.PointerEvent) => {
    e.preventDefault();
    pointerIdRef.current = e.pointerId;
    const rect = containerRef.current!.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    setPoints([{ x, y }]);
    setDirections([]);
    setIsDrawing(true);
    setMatched(false);
    lastDirRef.current = null;
    lastPointRef.current = { x, y };
  }, []);

  const handleReset = useCallback(() => {
    setPoints([]);
    setDirections([]);
    setIsDrawing(false);
    setMatched(false);
    lastDirRef.current = null;
    lastPointRef.current = null;
  }, []);

  const pointsStr = points.map((p) => `${p.x},${p.y}`).join(" ");

  const matchedActionName = (() => {
    if (!actionMap || directions.length === 0) return null;
    const key = directions.join(",");
    return actionMap[key] ?? null;
  })();

  return (
    <div className={`flex flex-col gap-2 ${className}`}>
      <div
        ref={containerRef}
        className="relative bg-base-200 rounded-lg border-2 border-dashed border-base-300 cursor-crosshair select-none overflow-hidden"
        style={{ height: 200 }}
        onPointerDown={handlePointerDown}
      >
        {points.length === 0 && !isDrawing && (
          <div className="absolute inset-0 flex items-center justify-center text-base-content/40 text-sm pointer-events-none">
            {t("tutorial.gestureCanvas.placeholder")}
          </div>
        )}
        {points.length > 0 && (
          <svg className="absolute inset-0 w-full h-full pointer-events-none">
            <polyline
              points={pointsStr}
              fill="none"
              stroke={trailColor}
              strokeWidth={trailWidth}
              strokeLinecap="round"
              strokeLinejoin="round"
            />
          </svg>
        )}
        {matched && (
          <div className="absolute inset-0 flex items-center justify-center bg-success/20">
            <span className="text-success font-bold text-lg">
              {t("tutorial.gestureCanvas.success")}
            </span>
          </div>
        )}
      </div>

      <div className="flex items-center gap-2 min-h-[32px]">
        {directions.length > 0 && (
          <div className="flex items-center gap-1.5 flex-wrap">
            <span className="text-xs text-base-content/60">
              {t("tutorial.gestureCanvas.detected", {
                pattern: directions.map((d) => directionArrows[d]).join(" "),
              })}
            </span>
            {matchedActionName && (
              <span className="badge badge-sm badge-primary">
                {t("tutorial.gestureCanvas.matched", { action: matchedActionName })}
              </span>
            )}
            {targetPattern && matched && (
              <span className="badge badge-sm badge-success">✓</span>
            )}
          </div>
        )}
        {(directions.length > 0 || points.length > 0) && (
          <button
            type="button"
            onClick={handleReset}
            className="btn btn-xs btn-ghost ml-auto"
          >
            {t("tutorial.gestureCanvas.tryAgain")}
          </button>
        )}
      </div>
    </div>
  );
}
