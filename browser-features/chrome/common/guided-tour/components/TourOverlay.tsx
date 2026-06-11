// SPDX-License-Identifier: MPL-2.0

import { createEffect, createMemo, createSignal, Show } from "solid-js";
import { Portal } from "solid-js/web";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type { TourController } from "../controller.ts";
import type { TargetRect, TooltipPlacement } from "../types.ts";

const SPOTLIGHT_PADDING = 6;
const TOOLTIP_GAP = 12;
const VIEWPORT_MARGIN = 8;

interface TooltipPosition {
  left: number;
  top: number;
}

interface TourTexts {
  next: string;
  previous: string;
  skip: string;
  finish: string;
  tryIt: string;
  progress: (current: number, total: number) => string;
}

function computeTooltipPosition(
  rect: TargetRect | null,
  placement: TooltipPlacement,
  tooltipW: number,
  tooltipH: number,
  viewportW: number,
  viewportH: number,
): TooltipPosition {
  if (!rect || placement === "center") {
    return {
      left: (viewportW - tooltipW) / 2,
      top: (viewportH - tooltipH) / 2,
    };
  }

  const pad = SPOTLIGHT_PADDING + TOOLTIP_GAP;
  const centerX = rect.x + rect.width / 2;
  const centerY = rect.y + rect.height / 2;

  const positions: Record<
    Exclude<TooltipPlacement, "center">,
    TooltipPosition
  > = {
    top: { left: centerX - tooltipW / 2, top: rect.y - pad - tooltipH },
    bottom: { left: centerX - tooltipW / 2, top: rect.y + rect.height + pad },
    left: { left: rect.x - pad - tooltipW, top: centerY - tooltipH / 2 },
    right: { left: rect.x + rect.width + pad, top: centerY - tooltipH / 2 },
  };

  // 画面外にはみ出す場合は反対側へ flip
  const fits = (p: TooltipPosition) =>
    p.left >= VIEWPORT_MARGIN &&
    p.top >= VIEWPORT_MARGIN &&
    p.left + tooltipW <= viewportW - VIEWPORT_MARGIN &&
    p.top + tooltipH <= viewportH - VIEWPORT_MARGIN;

  const flipOf: Record<string, TooltipPlacement> = {
    top: "bottom",
    bottom: "top",
    left: "right",
    right: "left",
  };

  let pos = positions[placement];
  if (!fits(pos)) {
    const flipped = positions[
      flipOf[placement] as Exclude<TooltipPlacement, "center">
    ];
    if (fits(flipped)) pos = flipped;
  }

  // 最終的に画面内へクランプ
  return {
    left: Math.min(
      Math.max(pos.left, VIEWPORT_MARGIN),
      viewportW - tooltipW - VIEWPORT_MARGIN,
    ),
    top: Math.min(
      Math.max(pos.top, VIEWPORT_MARGIN),
      viewportH - tooltipH - VIEWPORT_MARGIN,
    ),
  };
}

export function createTourOverlay(controller: TourController) {
  return function TourOverlay() {
    const [texts, setTexts] = createSignal<TourTexts>({
      next: "Next",
      previous: "Prev",
      skip: "Skip",
      finish: "Got it",
      tryIt: "Try it yourself!",
      progress: (c, t) => `${c} / ${t}`,
    });
    addI18nObserver(() => {
      setTexts({
        next: i18next.t("guidedTour.next"),
        previous: i18next.t("guidedTour.previous"),
        skip: i18next.t("guidedTour.skip"),
        finish: i18next.t("guidedTour.finish"),
        tryIt: i18next.t("guidedTour.tryIt"),
        progress: (current, total) =>
          i18next.t("guidedTour.progress", { current, total }),
      });
    });

    const [tooltipSize, setTooltipSize] = createSignal({ w: 320, h: 160 });

    const visible = () =>
      controller.status() === "active" &&
      controller.tour() !== null && controller.stepReady();

    const step = () => controller.currentStep();

    const spotlight = createMemo<TargetRect | null>(() => {
      const rect = controller.targetRect();
      if (!visible() || !step()?.selector || !rect) return null;
      return {
        x: rect.x - SPOTLIGHT_PADDING,
        y: rect.y - SPOTLIGHT_PADDING,
        width: rect.width + SPOTLIGHT_PADDING * 2,
        height: rect.height + SPOTLIGHT_PADDING * 2,
      };
    });

    // ツールチップの実サイズを測る（文言・ステップ変更時）
    // ref はカスタムレンダラー (@nora/solid-xul) 未対応のため id で参照する
    createEffect(() => {
      void step();
      void texts();
      void visible();
      requestAnimationFrame(() => {
        const el = document.getElementById("floorp-guided-tour-tooltip");
        if (el) {
          const r = el.getBoundingClientRect();
          if (r.width > 0 && r.height > 0) {
            setTooltipSize({ w: r.width, h: r.height });
          }
        }
      });
    });

    const tooltipPos = createMemo<TooltipPosition>(() => {
      const s = step();
      return computeTooltipPosition(
        spotlight(),
        s?.placement ?? "center",
        tooltipSize().w,
        tooltipSize().h,
        window.innerWidth,
        window.innerHeight,
      );
    });

    const progressText = () => {
      const tour = controller.tour();
      if (!tour) return "";
      return texts().progress(controller.stepIndex() + 1, tour.steps.length);
    };

    // 4 枚のブロッカーでスポットライト外の入力を遮断する。
    // passthrough のときはスポットライト内が素通しになり実操作できる。
    const blockers = createMemo(() => {
      const s = spotlight();
      const w = window.innerWidth;
      const h = window.innerHeight;
      if (!s) {
        return [{ left: 0, top: 0, width: w, height: h }];
      }
      return [
        { left: 0, top: 0, width: w, height: Math.max(s.y, 0) },
        {
          left: 0,
          top: s.y + s.height,
          width: w,
          height: Math.max(h - s.y - s.height, 0),
        },
        { left: 0, top: s.y, width: Math.max(s.x, 0), height: s.height },
        {
          left: s.x + s.width,
          top: s.y,
          width: Math.max(w - s.x - s.width, 0),
          height: s.height,
        },
      ];
    });

    return (
      <Portal mount={document.getElementById("main-window") ?? undefined}>
        <Show when={visible()}>
          <div id="floorp-guided-tour-overlay">
            {/* 入力ブロッカー（dim なし、イベント遮断のみ） */}
            {blockers().map((b) => (
              <div
                class="floorp-guided-tour-blocker"
                style={{
                  left: `${b.left}px`,
                  top: `${b.top}px`,
                  width: `${b.width}px`,
                  height: `${b.height}px`,
                }}
              />
            ))}

            {/* スポットライト: box-shadow で周囲を dim */}
            <Show
              when={spotlight()}
              fallback={<div class="floorp-guided-tour-dim" />}
            >
              {(s) => (
                <div
                  class="floorp-guided-tour-spotlight"
                  data-passthrough={step()?.passthrough ? "true" : undefined}
                  style={{
                    left: `${s().x}px`,
                    top: `${s().y}px`,
                    width: `${s().width}px`,
                    height: `${s().height}px`,
                  }}
                />
              )}
            </Show>

            {/* passthrough でない場合はスポットライト内のクリックも遮断 */}
            <Show when={spotlight() && !step()?.passthrough}>
              <div
                class="floorp-guided-tour-blocker"
                style={{
                  left: `${spotlight()!.x}px`,
                  top: `${spotlight()!.y}px`,
                  width: `${spotlight()!.width}px`,
                  height: `${spotlight()!.height}px`,
                }}
              />
            </Show>

            <div
              id="floorp-guided-tour-tooltip"
              class="floorp-guided-tour-tooltip"
              role="dialog"
              aria-modal="false"
              style={{
                left: `${tooltipPos().left}px`,
                top: `${tooltipPos().top}px`,
              }}
            >
              <div class="floorp-guided-tour-progress">{progressText()}</div>
              <h2 class="floorp-guided-tour-title">
                {i18next.t(step()?.titleKey ?? "")}
              </h2>
              <p class="floorp-guided-tour-description">
                {i18next.t(step()?.descriptionKey ?? "")}
              </p>
              <Show when={step()?.passthrough}>
                <p class="floorp-guided-tour-tryit">{texts().tryIt}</p>
              </Show>
              <div class="floorp-guided-tour-buttons">
                <Show when={!controller.isLastStep()}>
                  <button
                    type="button"
                    class="floorp-guided-tour-btn floorp-guided-tour-btn-ghost"
                    onClick={() => controller.stop()}
                  >
                    {texts().skip}
                  </button>
                </Show>
                <div class="floorp-guided-tour-buttons-right">
                  <Show when={controller.stepIndex() > 0}>
                    <button
                      type="button"
                      class="floorp-guided-tour-btn floorp-guided-tour-btn-ghost"
                      onClick={() => controller.prev()}
                    >
                      {texts().previous}
                    </button>
                  </Show>
                  <button
                    type="button"
                    class="floorp-guided-tour-btn floorp-guided-tour-btn-primary"
                    onClick={() => controller.next()}
                  >
                    {controller.isLastStep() ? texts().finish : texts().next}
                  </button>
                </div>
              </div>
            </div>
          </div>
        </Show>
      </Portal>
    );
  };
}
