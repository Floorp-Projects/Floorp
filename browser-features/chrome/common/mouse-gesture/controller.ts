/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type GestureDirection,
  getConfig,
  isEnabled,
  patternToString,
} from "./config.ts";
import { GestureDisplay } from "./components/GestureDisplay.tsx";
import {
  executeGestureAction,
  getActionDisplayName,
} from "./utils/gestures.ts";

export class MouseGestureController {
  private isGestureActive = false;
  private isContextMenuPrevented = false;
  private preventionTimeoutId: number | null = null;
  private gesturePattern: GestureDirection[] = [];
  private lastX = 0;
  private lastY = 0;
  private accumulatedDX = 0;
  private accumulatedDY = 0;
  private mouseTrail: { x: number; y: number }[] = [];
  private display: GestureDisplay;
  private activeActionName = "";
  private eventListenersAttached = false;

  constructor() {
    this.display = new GestureDisplay();
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    if (typeof globalThis !== "undefined") {
      globalThis.addEventListener("mousedown", this.handleMouseDown);
      globalThis.addEventListener("mousemove", this.handleMouseMove);
      globalThis.addEventListener("mouseup", this.handleMouseUp);
      globalThis.addEventListener("contextmenu", this.handleContextMenu, true);
      this.eventListenersAttached = true;
    }
  }

  public destroy(): void {
    if (typeof globalThis !== "undefined" && this.eventListenersAttached) {
      globalThis.removeEventListener("mousedown", this.handleMouseDown);
      globalThis.removeEventListener("mousemove", this.handleMouseMove);
      globalThis.removeEventListener("mouseup", this.handleMouseUp);
      globalThis.removeEventListener(
        "contextmenu",
        this.handleContextMenu,
        true,
      );
      this.eventListenersAttached = false;
    }

    if (this.preventionTimeoutId !== null) {
      clearTimeout(this.preventionTimeoutId);
      this.preventionTimeoutId = null;
    }

    this.resetGestureState();
    this.display.destroy();
  }

  private getAdjustedClientCoords(event: MouseEvent): { x: number; y: number } {
    return {
      x: event.clientX,
      y: event.clientY,
    };
  }

  private handleMouseDown = (event: MouseEvent): void => {
    if (event.button !== 2 || !isEnabled()) {
      return;
    }

    this.isContextMenuPrevented = true;
    if (this.preventionTimeoutId !== null) {
      clearTimeout(this.preventionTimeoutId);
      this.preventionTimeoutId = null;
    }

    this.isGestureActive = true;
    this.gesturePattern = [];

    const coords = this.getAdjustedClientCoords(event);
    this.lastX = coords.x;
    this.lastY = coords.y;
    this.accumulatedDX = 0;
    this.accumulatedDY = 0;
    this.mouseTrail = [coords];
    this.activeActionName = "";

    this.display.show();
    this.display.updateTrail(this.mouseTrail);

    if (event.target instanceof Element) {
      event.preventDefault();
    }
  };

  private handleMouseMove = (event: MouseEvent): void => {
    if (!this.isGestureActive || !isEnabled()) {
      return;
    }

    const config = getConfig();
    const coords = this.getAdjustedClientCoords(event);
    this.mouseTrail.push(coords);

    // パフォーマンス向上のため、一定のポイント数毎にパターンマッチングを実行
    if (this.mouseTrail.length % 3 === 0 || this.mouseTrail.length > 20) {
      const recognizedPattern = this.recognizeGesturePattern(config);
      if (recognizedPattern) {
        const matchingAction = this.findMatchingAction(
          config,
          recognizedPattern,
        );
        if (matchingAction) {
          this.gesturePattern = recognizedPattern;
          this.activeActionName = getActionDisplayName(matchingAction.action);
        } else {
          this.activeActionName = "";
        }
      } else {
        this.activeActionName = "";
      }
    }

    this.lastX = coords.x;
    this.lastY = coords.y;
    this.display.updateTrail(this.mouseTrail);
    this.display.updateActionName(this.activeActionName);
  };

  private handleMouseUp = (event: MouseEvent): void => {
    if (!this.isGestureActive || event.button !== 2 || !isEnabled()) return;

    const config = getConfig();
    const preventionTimeout = config.contextMenu.preventionTimeout;

    // 最終的なパターン認識を実行
    const finalPattern = this.recognizeGesturePattern(config);

    if (finalPattern && finalPattern.length > 0) {
      const matchingAction = this.findMatchingAction(config, finalPattern);

      if (matchingAction) {
        this.gesturePattern = finalPattern;
        this.activeActionName = getActionDisplayName(matchingAction.action);
        this.display.updateActionName(this.activeActionName);
        setTimeout(() => {
          executeGestureAction(matchingAction.action);
          this.resetGestureState();
          this.preventionTimeoutId = setTimeout(() => {
            this.isContextMenuPrevented = false;
            this.preventionTimeoutId = null;
          }, preventionTimeout);
        }, 100);

        return;
      }
    }

    // ジェスチャーが認識されない場合、最小移動距離をチェック
    const totalMovement = this.getTotalMovement();
    const minMovement = Math.max(config.contextMenu.minDistance, 10); // 最小値を設定

    if (totalMovement < minMovement) {
      this.isContextMenuPrevented = false;
      this.resetGestureState();
      return;
    }

    this.preventionTimeoutId = setTimeout(() => {
      this.isContextMenuPrevented = false;
      this.preventionTimeoutId = null;
    }, preventionTimeout);

    this.resetGestureState();
  };

  private getTotalMovement(): number {
    if (this.mouseTrail.length < 2) return 0;

    const startPoint = this.mouseTrail[0];
    const lastPoint = this.mouseTrail[this.mouseTrail.length - 1];

    const dx = lastPoint.x - startPoint.x;
    const dy = lastPoint.y - startPoint.y;

    return Math.sqrt(dx * dx + dy * dy);
  }

  private resetGestureState(): void {
    this.isGestureActive = false;
    this.gesturePattern = [];
    this.accumulatedDX = 0;
    this.accumulatedDY = 0;
    this.mouseTrail = [];
    this.activeActionName = "";
    this.display.hide();
  }

  private handleContextMenu = (event: MouseEvent): void => {
    if ((this.isGestureActive || this.isContextMenuPrevented) && isEnabled()) {
      event.preventDefault();
      event.stopPropagation();
    }
  };

  private findMatchingAction(
    config: ReturnType<typeof getConfig>,
    pattern: GestureDirection[] = this.gesturePattern,
  ) {
    if (pattern.length === 0) {
      return null;
    }

    const patternString = patternToString(pattern);

    // 早期拒否: トレイル全体の方向とアクションの主要方向が明らかに異なる場合は無視
    // トレイルが十分な距離を移動しているか確認
    const totalTrailDistance = this.getTotalTrailDistance();
    const minTotalForMatch = Math.max(10, config.contextMenu.minDistance);

    for (const action of config.actions) {
      // 完全一致があれば即時返却
      if (patternToString(action.pattern) === patternString) {
        return action;
      }
    }

    // 明らかに違うジェスチャの早期拒否: 全体のトレイル方向ベクトルを計算
    if (totalTrailDistance < minTotalForMatch) {
      // 移動が小さすぎる場合はどのアクションともマッチしない
      return null;
    }

    const start = this.mouseTrail[0];
    const end = this.mouseTrail[this.mouseTrail.length - 1];
    const overallDx = end.x - start.x;
    const overallDy = end.y - start.y;
    const overallMag = Math.hypot(overallDx, overallDy);
    if (overallMag === 0) return null;

    const overallNx = overallDx / overallMag;
    const overallNy = overallDy / overallMag;

    // 各アクションの代表方向（パターンの最初の方向）とのコサイン類似度を確認
    // 感度に応じて閾値を変化させる（感度が高いほど厳しく）
    const sensitivityFactorForThreshold = (config.sensitivity || 40) / 100; // 0-1
    const earlyRejectCosineThreshold = Math.max(
      0.2,
      0.55 - sensitivityFactorForThreshold * 0.5,
    );

    const candidates = [] as typeof config.actions;
    for (const action of config.actions) {
      const repDir = action.pattern[0];
      if (!repDir) continue;
      const repVec = this.patternToVectors([repDir])[0];
      const cos = overallNx * repVec.dx + overallNy * repVec.dy;
      if (cos >= earlyRejectCosineThreshold) {
        candidates.push(action);
      }
    }

    if (candidates.length === 0) return null;

    // 候補のみについて詳細類似度を計算して最良を選択
    let best: (typeof config.actions)[number] | null = null;
    let bestSim = 0;
    for (const action of candidates) {
      const sim = this.calculatePatternSimilarity(action.pattern);
      if (sim > bestSim) {
        bestSim = sim;
        best = action;
      }
    }

    // 感度に応じた最小類似度を要求（感度が高いほど高い類似度を要求）
    const sensitivityFactor = (config.sensitivity || 40) / 100;
    const minSimilarity = Math.max(0.35, 0.6 + sensitivityFactor * 0.35);
    if (best && bestSim >= minSimilarity) return best;

    return null;
  }

  /**
   * 新しいジェスチャー認識アルゴリズム
   * 全体的なトレイルの形状を分析して、設定されたパターンとマッチするかを判定
   */
  private recognizeGesturePattern(
    config: ReturnType<typeof getConfig>,
  ): GestureDirection[] | null {
    if (this.mouseTrail.length < 2) {
      return null;
    }

    // 最小移動距離を大幅に削減（小さなジェスチャーに対応）
    const totalDistance = this.getTotalTrailDistance();
    const minDistance = Math.max(3, config.contextMenu.minDistance * 0.3); // より小さな最小距離
    if (totalDistance < minDistance) {
      return null;
    }

    // 各設定されたパターンとの類似度を計算
    const bestMatch = this.findBestPatternMatch(config);
    return bestMatch;
  }

  /**
   * トレイル全体の距離を計算
   */
  private getTotalTrailDistance(): number {
    if (this.mouseTrail.length < 2) return 0;

    let totalDistance = 0;
    for (let i = 1; i < this.mouseTrail.length; i++) {
      const prev = this.mouseTrail[i - 1];
      const curr = this.mouseTrail[i];
      totalDistance += Math.hypot(curr.x - prev.x, curr.y - prev.y);
    }
    return totalDistance;
  }

  /**
   * 設定されたパターンの中から最も類似度の高いものを探す
   */
  private findBestPatternMatch(
    config: ReturnType<typeof getConfig>,
  ): GestureDirection[] | null {
    const configActions = config.actions;
    if (configActions.length === 0) {
      return null;
    }

    let bestPattern: GestureDirection[] | null = null;
    let bestSimilarity = 0;

    // 感度に応じて類似度閾値を動的に調整
    const sensitivityFactor = (config.sensitivity || 40) / 100; // 0.0-1.0の範囲
    const minSimilarity = Math.max(0.4, 0.8 - sensitivityFactor * 0.3); // 0.4-0.8の範囲

    for (const action of configActions) {
      const similarity = this.calculatePatternSimilarity(action.pattern);
      if (similarity > bestSimilarity && similarity >= minSimilarity) {
        bestSimilarity = similarity;
        bestPattern = action.pattern;
      }
    }

    return bestPattern;
  }

  /**
   * 現在のトレイルと指定されたパターンの類似度を計算
   */
  private calculatePatternSimilarity(
    targetPattern: GestureDirection[],
  ): number {
    if (targetPattern.length === 0) return 0;

    // 単一方向のパターンは簡単な方法で判定
    if (targetPattern.length === 1) {
      return this.calculateSingleDirectionSimilarity(targetPattern[0]);
    }

    // トレイルを正規化されたベクトルに変換
    const normalizedTrail = this.normalizeTrail();
    if (normalizedTrail.length === 0) return 0;

    // パターンを理想的なベクトル系列に変換
    const idealVectors = this.patternToVectors(targetPattern);

    // DTW（Dynamic Time Warping）アルゴリズムで類似度を計算
    const similarity = this.calculateDTWSimilarity(
      normalizedTrail,
      idealVectors,
    );

    return similarity;
  }

  /**
   * 単一方向パターンの類似度を計算（より簡潔で高速）
   */
  private calculateSingleDirectionSimilarity(
    targetDirection: GestureDirection,
  ): number {
    // 単一方向パターンの類似度は、トレイル全体の各セグメントを
    // 方向ベクトルに投影して重み付き平均を取ることで評価する。
    // これにより開始点・終了点だけで判断するよりも全体の動きを反映できる。
    if (this.mouseTrail.length < 2) return 0;

    const trailVectors = this.normalizeTrail();
    if (trailVectors.length === 0) return 0;

    const targetVectors = this.patternToVectors([targetDirection]);
    if (targetVectors.length === 0) return 0;

    const targetVec = targetVectors[0];

    // 各トレイルベクトルのコサイン類似度を、そのセグメント長で重み付けして合算
    let weightedSum = 0;
    let totalWeight = 0;

    for (const seg of trailVectors) {
      const dot = seg.dx * targetVec.dx + seg.dy * targetVec.dy;
      // seg.magnitude は実際のピクセル距離なのでこれを重みに使う
      const weight = seg.magnitude;
      weightedSum += Math.max(0, dot) * weight; // 負の類似度は0にクリップ
      totalWeight += weight;
    }

    if (totalWeight === 0) return 0;

    const avgSimilarity = weightedSum / totalWeight; // 0-1
    return Math.max(0, Math.min(1, avgSimilarity));
  }

  /**
   * トレイルを正規化されたベクトル系列に変換
   */
  private normalizeTrail(): { dx: number; dy: number; magnitude: number }[] {
    if (this.mouseTrail.length < 2) return [];

    const vectors: { dx: number; dy: number; magnitude: number }[] = [];

    for (let i = 1; i < this.mouseTrail.length; i++) {
      const prev = this.mouseTrail[i - 1];
      const curr = this.mouseTrail[i];
      const dx = curr.x - prev.x;
      const dy = curr.y - prev.y;
      const magnitude = Math.hypot(dx, dy);

      if (magnitude > 0.1) {
        // 極めて小さな移動のみ無視（0.1ピクセル以下）
        vectors.push({
          dx: dx / magnitude,
          dy: dy / magnitude,
          magnitude,
        });
      }
    }

    return vectors;
  }

  /**
   * パターンを理想的なベクトル系列に変換
   */
  private patternToVectors(
    pattern: GestureDirection[],
  ): { dx: number; dy: number }[] {
    const directionMap: Record<GestureDirection, { dx: number; dy: number }> = {
      right: { dx: 1, dy: 0 },
      downRight: { dx: 0.707, dy: 0.707 },
      down: { dx: 0, dy: 1 },
      downLeft: { dx: -0.707, dy: 0.707 },
      left: { dx: -1, dy: 0 },
      upLeft: { dx: -0.707, dy: -0.707 },
      up: { dx: 0, dy: -1 },
      upRight: { dx: 0.707, dy: -0.707 },
    };

    return pattern.map((direction) => directionMap[direction]);
  }

  /**
   * DTW（Dynamic Time Warping）を使用した類似度計算
   */
  private calculateDTWSimilarity(
    trailVectors: { dx: number; dy: number; magnitude: number }[],
    idealVectors: { dx: number; dy: number }[],
  ): number {
    if (trailVectors.length === 0 || idealVectors.length === 0) return 0;

    const n = trailVectors.length;
    const m = idealVectors.length;

    // DTWマトリックス
    const dtw: number[][] = Array(n + 1)
      .fill(null)
      .map(() => Array(m + 1).fill(Number.POSITIVE_INFINITY));
    dtw[0][0] = 0;

    // トレイル側のセグメント長を考慮してコストに重み付けを行う
    const trailTotalWeight = Math.max(
      1,
      trailVectors.reduce((s, v) => s + v.magnitude, 0),
    );

    for (let i = 1; i <= n; i++) {
      for (let j = 1; j <= m; j++) {
        const trailVec = trailVectors[i - 1];
        const idealVec = idealVectors[j - 1];

        // コサイン類似度を計算
        const dotProduct =
          trailVec.dx * idealVec.dx + trailVec.dy * idealVec.dy;
        const cosineSimilarity = Math.max(0, dotProduct); // 0-1の範囲に正規化
        const localCost = 1 - cosineSimilarity;

        // 重み付きローカルコスト: セグメント長を重みとして利用
        const weightedLocalCost = localCost * trailVec.magnitude;

        dtw[i][j] =
          weightedLocalCost +
          Math.min(
            dtw[i - 1][j], // 挿入
            dtw[i][j - 1], // 削除
            dtw[i - 1][j - 1], // マッチ
          );
      }
    }

    // 正規化: 合計の重みで割ることで 0-1 程度に収める
    const normalizedDistance = dtw[n][m] / trailTotalWeight;
    return Math.max(0, 1 - normalizedDistance);
  }
}
