/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.ui;

import org.json.JSONObject;
import org.mozilla.gecko.gfx.FloatRect;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import android.graphics.PointF;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import java.util.Timer;
import java.util.TimerTask;

/*
 * Handles the kinetic scrolling and zooming physics for a layer controller.
 *
 * Many ideas are from Joe Hewitt's Scrollability:
 *   https://github.com/joehewitt/scrollability/
 */
public class PanZoomController
    extends GestureDetector.SimpleOnGestureListener
    implements ScaleGestureDetector.OnScaleGestureListener
{
    private static final String LOG_NAME = "PanZoomController";

    private LayerController mController;

    private static final float FRICTION = 0.97f;
    // Animation stops if the velocity is below this value.
    private static final float STOPPED_THRESHOLD = 4.0f;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static final float SNAP_LIMIT = 0.75f;
    // The rate of deceleration when the surface has overscrolled.
    private static final float OVERSCROLL_DECEL_RATE = 0.04f;
    // The duration of animation when bouncing back.
    private static final int SNAP_TIME = 150;
    // The number of subdivisions we should consider when plotting the ease-out transition. Higher
    // values make the animation more accurate, but slower to plot.
    private static final int SUBDIVISION_COUNT = 1000;
    // The distance the user has to pan before we recognize it as such (e.g. to avoid
    // 1-pixel pans between the touch-down and touch-up of a click)
    private static final float PAN_THRESHOLD = 4.0f;

    private long mLastTimestamp;
    private Timer mFlingTimer;
    private Axis mX, mY;
    /* The span at the first zoom event (in unzoomed page coordinates). */
    private float mInitialZoomSpan;
    /* The zoom focus at the first zoom event (in unzoomed page coordinates). */
    private PointF mInitialZoomFocus;

    private enum PanZoomState {
        NOTHING,        /* no touch-start events received */
        FLING,          /* all touches removed, but we're still scrolling page */
        TOUCHING,       /* one touch-start event received */
        PANNING,        /* touch-start followed by move */
        PANNING_HOLD,   /* in panning, but haven't moved.
                         * similar to TOUCHING but after starting a pan */
        PINCHING,       /* nth touch-start, where n > 1. this mode allows pan and zoom */
    }

    private PanZoomState mState;

    public PanZoomController(LayerController controller) {
        mController = controller;
        mX = new Axis(); mY = new Axis();
        mState = PanZoomState.NOTHING;

        populatePositionAndLength();
    }

    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:   return onTouchStart(event);
        case MotionEvent.ACTION_MOVE:   return onTouchMove(event);
        case MotionEvent.ACTION_UP:     return onTouchEnd(event);
        case MotionEvent.ACTION_CANCEL: return onTouchCancel(event);
        default:                        return false;
        }
    }

    public void geometryChanged() {
        populatePositionAndLength();
    }

    /*
     * Panning/scrolling
     */

    private boolean onTouchStart(MotionEvent event) {
        switch (mState) {
        case FLING:
            if (mFlingTimer != null) {
                mFlingTimer.cancel();
                mFlingTimer = null;
            }
            // fall through
        case NOTHING:
            mState = PanZoomState.TOUCHING;
            mX.touchPos = event.getX(0);
            mY.touchPos = event.getY(0);
            return false;
        case TOUCHING:
        case PANNING:
        case PANNING_HOLD:
        case PINCHING:
            mState = PanZoomState.PINCHING;
            return false;
        }
        Log.e(LOG_NAME, "Unhandled case " + mState + " in onTouchStart");
        return false;
    }

    private boolean onTouchMove(MotionEvent event) {
        switch (mState) {
        case NOTHING:
        case FLING:
            // should never happen
            Log.e(LOG_NAME, "Received impossible touch move while in " + mState);
            return false;
        case TOUCHING:
            mLastTimestamp = System.currentTimeMillis();
            if (panDistance(event) < PAN_THRESHOLD)
                return false;
            // fall through
        case PANNING_HOLD:
            mState = PanZoomState.PANNING;
            // fall through
        case PANNING:
            track(event, System.currentTimeMillis());
            return true;
        case PINCHING:
            // scale gesture listener will handle this
            return false;
        }
        Log.e(LOG_NAME, "Unhandled case " + mState + " in onTouchMove");
        return false;
    }

    private boolean onTouchEnd(MotionEvent event) {
        switch (mState) {
        case NOTHING:
        case FLING:
            // should never happen
            Log.e(LOG_NAME, "Received impossible touch end while in " + mState);
            return false;
        case TOUCHING:
            mState = PanZoomState.NOTHING;
            // TODO: send click to gecko
            return false;
        case PANNING:
        case PANNING_HOLD:
            mState = PanZoomState.FLING;
            fling(System.currentTimeMillis());
            return true;
        case PINCHING:
            int points = event.getPointerCount();
            if (points == 1) {
                // last touch up
                mState = PanZoomState.NOTHING;
            } else if (points == 2) {
                int pointRemovedIndex = event.getActionIndex();
                int pointRemainingIndex = 1 - pointRemovedIndex; // kind of a hack
                mState = PanZoomState.TOUCHING;
                mX.touchPos = event.getX(pointRemainingIndex);
                mY.touchPos = event.getY(pointRemainingIndex);
            } else {
                // still pinching, do nothing
            }
            return true;
        }
        Log.e(LOG_NAME, "Unhandled case " + mState + " in onTouchEnd");
        return false;
    }

    private boolean onTouchCancel(MotionEvent event) {
        mState = PanZoomState.NOTHING;
        return false;
    }

    private float panDistance(MotionEvent move) {
        float dx = mX.touchPos - move.getX(0);
        float dy = mY.touchPos - move.getY(0);
        return (float)Math.sqrt(dx * dx + dy * dy);
    }

    private void track(MotionEvent event, long timestamp) {
        long timeStep = timestamp - mLastTimestamp;
        mLastTimestamp = timestamp;

        float zoomFactor = mController.getZoomFactor();
        mX.velocity = (mX.touchPos - event.getX(0)) / zoomFactor;
        mY.velocity = (mY.touchPos - event.getY(0)) / zoomFactor;
        mX.touchPos = event.getX(0); mY.touchPos = event.getY(0);

        if (stopped())
            mState = PanZoomState.PANNING_HOLD;

        mX.applyEdgeResistance(); mX.displace();
        mY.applyEdgeResistance(); mY.displace();
        updatePosition();
    }

    private void fling(long timestamp) {
        long timeStep = timestamp - mLastTimestamp;
        mLastTimestamp = timestamp;

        if (mState != PanZoomState.FLING)
            mX.velocity = mY.velocity = 0.0f;

        mX.displace(); mY.displace();

        if (mFlingTimer != null)
            mFlingTimer.cancel();

        boolean stopped = stopped();
        mX.startFling(stopped); mY.startFling(stopped);

        mFlingTimer = new Timer();
        mFlingTimer.scheduleAtFixedRate(new TimerTask() {
            public void run() { mController.post(new FlingRunnable()); }
        }, 0, 1000L/60L);
    }

    private boolean stopped() {
        float absVelocity = (float)Math.sqrt(mX.velocity * mX.velocity +
                                             mY.velocity * mY.velocity);
        return absVelocity < STOPPED_THRESHOLD;
    }

    private void updatePosition() {
        mController.scrollTo(mX.viewportPos, mY.viewportPos);
        mController.notifyLayerClientOfGeometryChange();
    }

    // Populates the viewport info and length in the axes.
    private void populatePositionAndLength() {
        IntSize pageSize = mController.getPageSize();
        FloatRect visibleRect = mController.getVisibleRect();
        IntSize screenSize = mController.getScreenSize();

        mX.setPageLength(pageSize.width);
        mX.viewportPos = visibleRect.x;
        mX.setViewportLength(visibleRect.width);

        mY.setPageLength(pageSize.height);
        mY.viewportPos = visibleRect.y;
        mY.setViewportLength(visibleRect.height);
    }

    // The callback that performs the fling animation.
    private class FlingRunnable implements Runnable {
        public void run() {
            populatePositionAndLength();
            mX.advanceFling(); mY.advanceFling();

            // If both X and Y axes are overscrolled, we have to wait until both axes have stopped
            // to snap back to avoid a jarring effect.
            boolean waitingToSnapX = mX.getFlingState() == Axis.FlingStates.WAITING_TO_SNAP;
            boolean waitingToSnapY = mY.getFlingState() == Axis.FlingStates.WAITING_TO_SNAP;
            if ((mX.getOverscroll() == Axis.Overscroll.PLUS || mX.getOverscroll() == Axis.Overscroll.MINUS) &&
                (mY.getOverscroll() == Axis.Overscroll.PLUS || mY.getOverscroll() == Axis.Overscroll.MINUS))
            {
                if (waitingToSnapX && waitingToSnapY) {
                    mX.startSnap(); mY.startSnap();
                }
            } else {
                if (waitingToSnapX)
                    mX.startSnap();
                if (waitingToSnapY)
                    mY.startSnap();
            }

            mX.displace(); mY.displace();
            updatePosition();

            if (mX.getFlingState() == Axis.FlingStates.STOPPED &&
                    mY.getFlingState() == Axis.FlingStates.STOPPED) {
                stop();
            }
        }

        private void stop() {
            mState = PanZoomState.NOTHING;
            if (mFlingTimer != null) {
                mFlingTimer.cancel();
                mFlingTimer = null;
            }
        }
    }

    private float computeElasticity(float excess, float viewportLength) {
        return 1.0f - excess / (viewportLength * SNAP_LIMIT);
    }

    // Physics information for one axis (X or Y).
    private static class Axis {
        public enum FlingStates {
            STOPPED,
            SCROLLING,
            WAITING_TO_SNAP,
            SNAPPING,
        }

        public enum Overscroll {
            NONE,
            MINUS,      // Overscrolled in the negative direction
            PLUS,       // Overscrolled in the positive direction
            BOTH,       // Overscrolled in both directions (page is zoomed to smaller than screen)
        }

        public float touchPos;                  /* Position of the last touch. */
        public float velocity;                  /* Velocity in this direction. */

        private FlingStates mFlingState;        /* The fling state we're in on this axis. */
        private EaseOutAnimation mSnapAnim;     /* The animation when the page is snapping back. */

        /* These three need to be kept in sync with the layer controller. */
        public float viewportPos;
        private float mViewportLength;
        private int mScreenLength;
        private int mPageLength;

        public FlingStates getFlingState() { return mFlingState; }

        public void setViewportLength(float viewportLength) { mViewportLength = viewportLength; }
        public void setScreenLength(int screenLength) { mScreenLength = screenLength; }
        public void setPageLength(int pageLength) { mPageLength = pageLength; }

        private float getViewportEnd() { return viewportPos + mViewportLength; }

        public Overscroll getOverscroll() {
            boolean minus = (viewportPos < 0.0f);
            boolean plus = (getViewportEnd() > mPageLength);
            if (minus && plus)
                return Overscroll.BOTH;
            else if (minus)
                return Overscroll.MINUS;
            else if (plus)
                return Overscroll.PLUS;
            else
                return Overscroll.NONE;
        }

        // Returns the amount that the page has been overscrolled. If the page hasn't been
        // overscrolled on this axis, returns 0.
        private float getExcess() {
            switch (getOverscroll()) {
            case MINUS:     return Math.min(-viewportPos, mPageLength - getViewportEnd());
            case PLUS:      return Math.min(viewportPos, getViewportEnd() - mPageLength);
            default:        return 0.0f;
            }
        }

        // Applies resistance along the edges when tracking.
        public void applyEdgeResistance() {
            float excess = getExcess();
            if (excess > 0.0f)
                velocity *= SNAP_LIMIT - excess / mViewportLength;
        }

        public void startFling(boolean stopped) {
            if (!stopped) {
                mFlingState = FlingStates.SCROLLING;
                return;
            }

            float excess = getExcess();
            if (excess == 0.0f)
                mFlingState = FlingStates.STOPPED;
            else
                mFlingState = FlingStates.WAITING_TO_SNAP;
        }

        // Advances a fling animation by one step.
        public void advanceFling() {
            switch (mFlingState) {
            case SCROLLING:
                scroll();
                return;
            case WAITING_TO_SNAP:
                // We don't do anything until the controller switches us into the snapping state.
                return;
            case SNAPPING:
                snap();
                return;
            }
        }

        // Performs one frame of a scroll operation if applicable.
        private void scroll() {
            // If we aren't overscrolled, just apply friction.
            float excess = getExcess();
            if (excess == 0.0f) {
                velocity *= FRICTION;
                if (Math.abs(velocity) < 0.1f) {
                    velocity = 0.0f;
                    mFlingState = FlingStates.STOPPED;
                }
                return;
            }

            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - excess / (mViewportLength * SNAP_LIMIT);
            if (getOverscroll() == Overscroll.MINUS)
                velocity = Math.min((velocity + OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            else // must be Overscroll.PLUS
                velocity = Math.max((velocity - OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);

            if (Math.abs(velocity) < 0.3f) {
                velocity = 0.0f;
                mFlingState = FlingStates.WAITING_TO_SNAP;
            }
        }

        // Starts a snap-into-place operation.
        public void startSnap() {
            switch (getOverscroll()) {
            case MINUS:
                mSnapAnim = new EaseOutAnimation(viewportPos, viewportPos + getExcess());
                break;
            case PLUS:
                mSnapAnim = new EaseOutAnimation(viewportPos, viewportPos - getExcess());
                break;
            default:
                throw new RuntimeException("Not overscrolled at startSnap()");
            }

            mFlingState = FlingStates.SNAPPING;
        }

        // Performs one frame of a snap-into-place operation.
        private void snap() {
            mSnapAnim.advance();
            viewportPos = mSnapAnim.getPosition();

            if (mSnapAnim.getFinished()) {
                mSnapAnim = null;
                mFlingState = FlingStates.STOPPED;
            }
        }

        // Performs displacement of the viewport position according to the current velocity.
        public void displace() { viewportPos += velocity; }
    }

    private static class EaseOutAnimation {
        private float[] mFrames;
        private float mPosition;
        private float mOrigin;
        private float mDest;
        private long mTimestamp;
        private boolean mFinished;

        public EaseOutAnimation(float position, float dest) {
            mPosition = mOrigin = position;
            mDest = dest;
            mFrames = new float[SNAP_TIME];
            mTimestamp = System.currentTimeMillis();
            mFinished = false;
            plot(position, dest, mFrames);
        }

        public float getPosition() { return mPosition; }
        public boolean getFinished() { return mFinished; }

        private void advance() {
            int frame = (int)(System.currentTimeMillis() - mTimestamp);
            if (frame >= SNAP_TIME) {
                mPosition = mDest;
                mFinished = true;
                return;
            }

            mPosition = mFrames[frame];
        }

        private static void plot(float from, float to, float[] frames) {
            int nextX = 0;
            for (int i = 0; i < SUBDIVISION_COUNT; i++) {
                float t = (float)i / (float)SUBDIVISION_COUNT;
                float xPos = (3.0f*t*t - 2.0f*t*t*t) * (float)frames.length;
                if ((int)xPos < nextX)
                    continue;

                int oldX = nextX;
                nextX = (int)xPos;

                float yPos = 1.74f*t*t - 0.74f*t*t*t;
                float framePos = from + (to - from) * yPos;

                while (oldX < nextX)
                    frames[oldX++] = framePos;

                if (nextX >= frames.length)
                    break;
            }

            // Pad out any remaining frames.
            while (nextX < frames.length) {
                frames[nextX] = frames[nextX - 1];
                nextX++;
            }
        }
    }

    /*
     * Zooming
     */
    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        mState = PanZoomState.PINCHING;
        float newZoom = detector.getCurrentSpan() / mInitialZoomSpan;

        IntSize screenSize = mController.getScreenSize();
        float x = mInitialZoomFocus.x - (detector.getFocusX() / newZoom);
        float y = mInitialZoomFocus.y - (detector.getFocusY() / newZoom);
        float width = screenSize.width / newZoom;
        float height = screenSize.height / newZoom;
        mController.setVisibleRect(x, y, width, height);
        mController.notifyLayerClientOfGeometryChange();
        populatePositionAndLength();
        return true;
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        mState = PanZoomState.PINCHING;
        FloatRect initialZoomRect = mController.getVisibleRect();
        float initialZoom = mController.getZoomFactor();

        mInitialZoomFocus = new PointF(initialZoomRect.x + (detector.getFocusX() / initialZoom),
                                       initialZoomRect.y + (detector.getFocusY() / initialZoom));
        mInitialZoomSpan = detector.getCurrentSpan() / initialZoom;
        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        mState = PanZoomState.PANNING_HOLD;
        mLastTimestamp = System.currentTimeMillis();
        mX.touchPos = detector.getFocusX();
        mY.touchPos = detector.getFocusY();
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        JSONObject ret = new JSONObject();
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            ret.put("x", (int)Math.round(point.x));
            ret.put("y", (int)Math.round(point.y));
        } catch(Exception ex) {
            Log.w(LOG_NAME, "Error building return: " + ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:LongPress", ret.toString());
        GeckoAppShell.sendEventToGecko(e);
    }
}
