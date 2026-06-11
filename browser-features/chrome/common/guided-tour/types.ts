// SPDX-License-Identifier: MPL-2.0

/** ツアー起動リクエスト用の one-shot pref。chrome 側が消費後すぐクリアする */
export const TOUR_REQUEST_PREF = "floorp.guidedTour.request";

/** pref に書き込まれる起動リクエストの形 */
export interface TourRequest {
  tourId: string;
}

export type TooltipPlacement = "top" | "bottom" | "left" | "right" | "center";

export type TourAction =
  | { type: "click"; selector: string }
  | { type: "rightClick"; selector: string }
  | { type: "scrollIntoView"; selector: string };

/**
 * ユーザーが実際に UI を操作したら次のステップへ進むトリガー。
 * event はターゲット上で capture フェーズで監視される。
 */
export interface AdvanceTrigger {
  event: string;
  selector: string;
}

export interface TourStep {
  /** スポットライトを当てる要素。null なら中央カードのみ表示 */
  selector: string | null;
  titleKey: string;
  descriptionKey: string;
  placement: TooltipPlacement;
  /** ステップ表示前に実行するアクション（パネルを開く等） */
  action?: TourAction;
  /** この要素が可視になるまで待ってから表示する */
  waitForSelector?: string;
  /** waitForSelector のタイムアウト (ms)。デフォルト 3000 */
  waitTimeout?: number;
  /** true ならスポットライト内をクリック可能にする（実操作を促す） */
  passthrough?: boolean;
  /** ユーザー操作による自動進行 */
  advanceOn?: AdvanceTrigger;
}

export interface TourDefinition {
  id: string;
  steps: TourStep[];
}

export type TourStatus = "idle" | "active" | "error";

export interface TargetRect {
  x: number;
  y: number;
  width: number;
  height: number;
}
