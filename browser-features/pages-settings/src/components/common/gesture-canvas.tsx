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

// ---- Ported from chrome/common/mouse-gesture/utils/recognizer.ts ----

const TWO_PI = 2 * Math.PI;
const ANGLE_SECTOR_HALF_WIDTH = Math.PI / 8;

function angleToDirection(angle: number): GestureDirection {
  const a = angle < 0 ? angle + TWO_PI : angle;
  if (a < ANGLE_SECTOR_HALF_WIDTH || a >= TWO_PI - ANGLE_SECTOR_HALF_WIDTH) return "right";
  if (a < 3 * ANGLE_SECTOR_HALF_WIDTH) return "downRight";
  if (a < 5 * ANGLE_SECTOR_HALF_WIDTH) return "down";
  if (a < 7 * ANGLE_SECTOR_HALF_WIDTH) return "downLeft";
  if (a < 9 * ANGLE_SECTOR_HALF_WIDTH) return "left";
  if (a < 11 * ANGLE_SECTOR_HALF_WIDTH) return "upLeft";
  if (a < 13 * ANGLE_SECTOR_HALF_WIDTH) return "up";
  return "upRight";
}

function deltaToDirection(dx: number, dy: number): GestureDirection {
  return angleToDirection(Math.atan2(dy, dx));
}

const MIN_SEGMENT_LENGTH = 25;
const MIN_POINTS_FOR_DIR_CHANGE = 3;

function extractDirectionSequence(trail: { x: number; y: number }[]): GestureDirection[] {
  if (trail.length < 2) return [];

  const directions: GestureDirection[] = [];
  let segStart = 0;
  let curDir: GestureDirection | null = null;
  let ptsInDir = 0;

  for (let i = 1; i < trail.length; i++) {
    const dx = trail[i].x - trail[segStart].x;
    const dy = trail[i].y - trail[segStart].y;
    const len = Math.sqrt(dx * dx + dy * dy);

    if (len >= MIN_SEGMENT_LENGTH) {
      const dir = deltaToDirection(dx, dy);

      if (curDir === null) {
        curDir = dir;
        ptsInDir = 1;
      } else if (dir === curDir) {
        ptsInDir++;
      } else {
        if (ptsInDir >= MIN_POINTS_FOR_DIR_CHANGE) {
          if (directions.length === 0 || directions[directions.length - 1] !== curDir) {
            directions.push(curDir);
          }
        }
        curDir = dir;
        ptsInDir = 1;
        segStart = i - 1;
      }
    }
  }

  if (curDir !== null && ptsInDir >= MIN_POINTS_FOR_DIR_CHANGE) {
    if (directions.length === 0 || directions[directions.length - 1] !== curDir) {
      directions.push(curDir);
    }
  }

  if (directions.length === 0 && curDir !== null) {
    directions.push(curDir);
  }

  return directions;
}

// ---- Component ----

export function GestureCanvas({
  onGestureComplete,
  targetPattern,
  trailColor = "#37ff00",
  trailWidth = 6,
  className = "",
  actionMap,
}: GestureCanvasProps) {
  const { t } = useTranslation();
  const [points, setPoints] = useState<{ x: number; y: number }[]>([]);
  const [directions, setDirections] = useState<GestureDirection[]>([]);
  const [isDrawing, setIsDrawing] = useState(false);
  const [matched, setMatched] = useState(false);
  const [actionPopup, setActionPopup] = useState<string | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const pointerIdRef = useRef<number | null>(null);
  const popupTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const trailRef = useRef<{ x: number; y: number }[]>([]);

  const completeGesture = useCallback(
    (dirs: GestureDirection[]) => {
      if (dirs.length > 0) {
        onGestureComplete(dirs);
        if (targetPattern && dirs.length === targetPattern.length) {
          const isMatch = dirs.every((d, i) => d === targetPattern[i]);
          if (isMatch) setMatched(true);
        }
        if (actionMap) {
          const key = dirs.join(",");
          const actionName = actionMap[key];
          if (actionName) {
            setActionPopup(actionName);
            if (popupTimerRef.current) clearTimeout(popupTimerRef.current);
            popupTimerRef.current = setTimeout(() => setActionPopup(null), 1500);
          }
        }
      }
    },
    [onGestureComplete, targetPattern, actionMap],
  );

  useEffect(() => {
    if (!isDrawing) return;
    const el = containerRef.current;
    if (!el || pointerIdRef.current === null) return;

    const onMove = (e: PointerEvent) => {
      const rect = el.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      // Skip negligible movement
      const last = trailRef.current[trailRef.current.length - 1];
      if (last && Math.hypot(x - last.x, y - last.y) < 1.5) return;

      trailRef.current.push({ x, y });
      setPoints([...trailRef.current]);
      setDirections(extractDirectionSequence(trailRef.current));
    };

    const onUp = () => {
      const dirs = extractDirectionSequence(trailRef.current);
      setDirections(dirs);
      completeGesture(dirs);
      setIsDrawing(false);
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
    if (e.button !== 2) return;
    e.preventDefault();
    pointerIdRef.current = e.pointerId;
    const rect = containerRef.current!.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    trailRef.current = [{ x, y }];
    setPoints([{ x, y }]);
    setDirections([]);
    setIsDrawing(true);
    setMatched(false);
    setActionPopup(null);
  }, []);

  const handleContextMenu = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
  }, []);

  const handleReset = useCallback(() => {
    trailRef.current = [];
    setPoints([]);
    setDirections([]);
    setIsDrawing(false);
    setMatched(false);
    setActionPopup(null);
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
        style={{ height: 140 }}
        onPointerDown={handlePointerDown}
        onContextMenu={handleContextMenu}
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
              opacity={0.9}
            />
          </svg>
        )}
        {actionPopup && !isDrawing && (
          <div className="absolute inset-0 flex items-center justify-center">
            <div
              className="px-4 py-2 text-white font-bold"
              style={{ backgroundColor: "rgba(0,0,0,0.7)", fontSize: 20 }}
            >
              {actionPopup}
            </div>
          </div>
        )}
        {matched && !actionPopup && (
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
