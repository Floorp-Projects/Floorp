// SPDX-License-Identifier: MPL-2.0

import { type Accessor, createSignal, onCleanup } from "solid-js";
import { getTourById } from "./tour-definitions/index.ts";
import { WorkspacePanelGuard } from "./workspace-panel-guard.ts";
import {
  type TargetRect,
  TOUR_REQUEST_PREF,
  type TourAction,
  type TourDefinition,
  type TourRequest,
  type TourStatus,
  type TourStep,
} from "./types.ts";

const WAIT_DEFAULT_TIMEOUT = 3000;

/**
 * ガイドツアーの状態機械。
 * - 起動は one-shot pref (`floorp.guidedTour.request`) を消費して行う
 * - 以降のステップ遷移はすべてメモリ内 (Solid signals) で完結する
 */
export class TourController {
  readonly tour: Accessor<TourDefinition | null>;
  readonly stepIndex: Accessor<number>;
  readonly status: Accessor<TourStatus>;
  readonly targetRect: Accessor<TargetRect | null>;
  readonly stepReady: Accessor<boolean>;

  private setTour: (v: TourDefinition | null) => void;
  private setStepIndex: (v: number) => void;
  private setStatus: (v: TourStatus) => void;
  private setTargetRect: (v: TargetRect | null) => void;
  private setStepReady: (v: boolean) => void;

  /** 非同期ステップ準備のレースを防ぐ世代カウンター */
  private generation = 0;
  private rafId: number | null = null;
  private advanceCleanup: (() => void) | null = null;
  private readonly workspacePanelGuard = new WorkspacePanelGuard();
  private panelSyncTimers: number[] = [];
  private targetElement: Element | null = null;

  constructor() {
    const [tour, setTour] = createSignal<TourDefinition | null>(null);
    const [stepIndex, setStepIndex] = createSignal(0);
    const [status, setStatus] = createSignal<TourStatus>("idle");
    const [targetRect, setTargetRect] = createSignal<TargetRect | null>(null);
    const [stepReady, setStepReady] = createSignal(false);
    this.tour = tour;
    this.setTour = setTour;
    this.stepIndex = stepIndex;
    this.setStepIndex = setStepIndex;
    this.status = status;
    this.setStatus = setStatus;
    this.targetRect = targetRect;
    this.setTargetRect = setTargetRect;
    this.stepReady = stepReady;
    this.setStepReady = setStepReady;

    this.observeRequestPref();
    onCleanup(() => this.stop());
  }

  currentStep(): TourStep | null {
    const tour = this.tour();
    if (!tour) return null;
    return tour.steps[this.stepIndex()] ?? null;
  }

  isLastStep(): boolean {
    const tour = this.tour();
    return tour !== null && this.stepIndex() >= tour.steps.length - 1;
  }

  start(tourId: string): void {
    const tour = getTourById(tourId);
    if (!tour) {
      console.error("[GuidedTour] unknown tour:", tourId);
      return;
    }
    this.setTour(tour);
    this.setStatus("active");
    void this.showStep(0);
  }

  next(): void {
    if (this.isLastStep()) {
      this.stop();
      return;
    }
    const nextIndex = this.stepIndex() + 1;
    const nextStep = this.tour()?.steps[nextIndex];
    setTimeout(() => {
      void this.showStep(nextIndex).then(() => {
        if (nextStep?.keepWorkspacePanelOpen) {
          void this.workspacePanelGuard.ensureOpen();
        }
      });
    }, 0);
  }

  prev(): void {
    if (this.stepIndex() > 0) {
      const prevIndex = this.stepIndex() - 1;
      const prevStep = this.tour()?.steps[prevIndex];
      setTimeout(() => {
        void this.showStep(prevIndex).then(() => {
          if (prevStep?.keepWorkspacePanelOpen) {
            void this.workspacePanelGuard.ensureOpen();
          }
        });
      }, 0);
    }
  }

  stop(): void {
    this.generation++;
    this.clearPanelSyncTimers();
    this.stopTrackingTarget();
    this.detachAdvanceTrigger();
    WorkspacePanelGuard.cleanupStale();
    this.targetElement = null;
    this.setTargetRect(null);
    this.setStepReady(false);
    this.setTour(null);
    this.setStatus("idle");
  }

  private async showStep(index: number): Promise<void> {
    const tour = this.tour();
    if (!tour) return;
    const step = tour.steps[index];
    if (!step) {
      this.stop();
      return;
    }

    const gen = ++this.generation;
    this.detachAdvanceTrigger();

    const panelAlreadyOpen = step.waitForSelector
      ? this.queryVisible(step.waitForSelector) !== null
      : false;
    const needsAsyncPrep =
      (step.waitForSelector !== undefined && !panelAlreadyOpen) ||
      (step.action !== undefined && !panelAlreadyOpen);

    if (needsAsyncPrep) {
      this.setStepReady(false);
    }
    this.setStepIndex(index);

    try {
      // ターゲット待機が必要な場合: 既に可視ならアクションをスキップ
      // （トグル系アクションでパネルが閉じてしまうのを防ぐ）
      if (step.waitForSelector) {
        const existing = this.queryVisible(step.waitForSelector);
        if (!existing) {
          if (step.action) this.executeAction(step.action);
          const el = await this.waitForElement(
            step.waitForSelector,
            step.waitTimeout ?? WAIT_DEFAULT_TIMEOUT,
          );
          if (gen !== this.generation) return;
          if (!el) {
            console.error(
              "[GuidedTour] target did not appear:",
              step.waitForSelector,
            );
            this.setStatus("error");
            return;
          }
        }
      } else if (step.action) {
        this.executeAction(step.action);
      }

      if (gen !== this.generation) return;

      this.targetElement = step.selector
        ? document.querySelector(step.selector)
        : null;
      if (step.selector && !this.targetElement) {
        console.error("[GuidedTour] target not found:", step.selector);
        this.setStatus("error");
        return;
      }

      this.startTrackingTarget();
      this.attachAdvanceTrigger(step);
      this.syncWorkspacePanelForStep(step);
      this.setStatus("active");
      this.setStepReady(true);
    } catch (e) {
      console.error("[GuidedTour]", e, "showing step", index);
      if (gen === this.generation) this.setStatus("error");
    }
  }

  private executeAction(action: TourAction): void {
    const el = document.querySelector(action.selector);
    if (!el) {
      console.error("[GuidedTour] action target not found:", action.selector);
      return;
    }
    switch (action.type) {
      case "click":
        (el as HTMLElement).click();
        break;
      case "rightClick":
        el.dispatchEvent(
          new PointerEvent("contextmenu", {
            bubbles: true,
            cancelable: true,
            button: 2,
          }),
        );
        break;
      case "scrollIntoView":
        el.scrollIntoView({ behavior: "smooth", block: "center" });
        break;
    }
  }

  /** MutationObserver + タイムアウトで要素の可視化を待つ */
  private waitForElement(
    selector: string,
    timeout: number,
  ): Promise<Element | null> {
    return new Promise((resolve) => {
      const found = this.queryVisible(selector);
      if (found) {
        resolve(found);
        return;
      }
      let settled = false;
      const settle = (el: Element | null) => {
        if (settled) return;
        settled = true;
        observer.disconnect();
        clearTimeout(timer);
        resolve(el);
      };
      const root = document.documentElement;
      if (!root) {
        resolve(null);
        return;
      }
      const observer = new MutationObserver(() => {
        const el = this.queryVisible(selector);
        if (el) settle(el);
      });
      observer.observe(root, {
        childList: true,
        subtree: true,
        attributes: true,
      });
      const timer = setTimeout(() => settle(null), timeout);
    });
  }

  private queryVisible(selector: string): Element | null {
    const el = document.querySelector(selector);
    if (!el) return null;
    const rect = el.getBoundingClientRect();
    return rect.width > 0 && rect.height > 0 ? el : null;
  }

  /**
   * rAF でターゲット矩形を追従する。
   * XUL ポップアップの開閉やリサイズなど、あらゆる移動に追従できる。
   * 値が変化したときのみ signal を更新する。
   */
  private startTrackingTarget(): void {
    this.stopTrackingTarget();
    const tick = () => {
      const el = this.targetElement;
      if (el) {
        const r = el.getBoundingClientRect();
        const prev = this.targetRect();
        if (
          !prev ||
          prev.x !== r.x ||
          prev.y !== r.y ||
          prev.width !== r.width ||
          prev.height !== r.height
        ) {
          this.setTargetRect({
            x: r.x,
            y: r.y,
            width: r.width,
            height: r.height,
          });
        }
      } else if (this.targetRect() !== null) {
        this.setTargetRect(null);
      }
      this.rafId = requestAnimationFrame(tick);
    };
    this.rafId = requestAnimationFrame(tick);
  }

  private stopTrackingTarget(): void {
    if (this.rafId !== null) {
      cancelAnimationFrame(this.rafId);
      this.rafId = null;
    }
  }

  private attachAdvanceTrigger(step: TourStep): void {
    if (!step.advanceOn) return;
    const { event, selector } = step.advanceOn;
    const handler = (e: Event) => {
      const target = e.target as Element | null;
      // ツアー UI 自身のクリック（Next ボタン等）では進行しない
      if (target?.closest?.("#floorp-guided-tour-overlay")) return;
      if (target?.closest?.(selector)) {
        // 操作完了後に UI が更新されてから次ステップへ
        setTimeout(() => this.next(), 150);
      }
    };
    // ステップ遷移を引き起こした進行中のイベントに反応しないよう、
    // リスナーの登録を次のタスクまで遅延させる
    let attached = false;
    const timer = setTimeout(() => {
      document.addEventListener(event, handler, true);
      attached = true;
    }, 0);
    this.advanceCleanup = () => {
      clearTimeout(timer);
      if (attached) {
        document.removeEventListener(event, handler, true);
      }
    };
  }

  private detachAdvanceTrigger(): void {
    this.advanceCleanup?.();
    this.advanceCleanup = null;
  }

  private syncWorkspacePanelForStep(step: TourStep): void {
    if (!step.keepWorkspacePanelOpen) return;
    this.scheduleWorkspacePanelOpen();
  }

  private scheduleWorkspacePanelOpen(): void {
    if (!this.tour()) return;
    this.clearPanelSyncTimers();
    const open = () => {
      if (this.status() !== "active") return;
      const step = this.currentStep();
      if (!step?.keepWorkspacePanelOpen) return;
      void this.workspacePanelGuard.ensureOpen();
    };
    open();
    this.panelSyncTimers.push(setTimeout(open, 0));
    this.panelSyncTimers.push(setTimeout(open, 50));
    this.panelSyncTimers.push(setTimeout(open, 150));
  }

  private clearPanelSyncTimers(): void {
    for (const id of this.panelSyncTimers) {
      clearTimeout(id);
    }
    this.panelSyncTimers = [];
  }

  /** 起動リクエスト pref を監視し、消費したら即クリアする */
  private observeRequestPref(): void {
    const observer = () => {
      let raw = "";
      try {
        raw = Services.prefs.getStringPref(TOUR_REQUEST_PREF, "");
      } catch (e) {
        console.error("[GuidedTour]", e, "reading request pref");
        return;
      }
      if (!raw) return;
      try {
        Services.prefs.clearUserPref(TOUR_REQUEST_PREF);
      } catch (e) {
        console.error("[GuidedTour]", e, "clearing request pref");
      }
      try {
        const request = JSON.parse(raw) as TourRequest;
        if (typeof request.tourId === "string" && request.tourId) {
          this.start(request.tourId);
        }
      } catch (e) {
        console.error("[GuidedTour]", e, "parsing request pref:", raw);
      }
    };

    Services.prefs.addObserver(TOUR_REQUEST_PREF, observer);
    onCleanup(() => {
      Services.prefs.removeObserver(TOUR_REQUEST_PREF, observer);
    });
    // 起動時に既にリクエストが残っていれば消費する
    observer();
  }
}
