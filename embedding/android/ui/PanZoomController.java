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

package org.mozilla.fennec.ui;

import org.mozilla.fennec.gfx.IntPoint;
import org.mozilla.fennec.gfx.IntRect;
import org.mozilla.fennec.gfx.IntSize;
import org.mozilla.fennec.gfx.LayerController;
import android.util.Log;
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
public class PanZoomController {
    private LayerController mController;

    private static final float FRICTION = 0.97f;
    // Animation stops if the velocity is below this value.
    private static final int STOPPED_THRESHOLD = 4;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static final float SNAP_LIMIT = 0.75f;
    // The rate of deceleration when the surface has overscrolled.
    private static final float OVERSCROLL_DECEL_RATE = 0.04f;
    // The duration of animation when bouncing back.
    private static final int SNAP_TIME = 150;
    // The number of subdivisions we should consider when plotting the ease-out transition. Higher
    // values make the animation more accurate, but slower to plot.
    private static final int SUBDIVISION_COUNT = 1000;

    // The surface is still scrolling.
    private static final int FLING_STATE_SCROLLING = 0;
    // The surface is snapping back into place after overscrolling.
    private static final int FLING_STATE_SNAPPING = 1;

    private boolean mTouchMoved, mStopped;
    private long mLastTimestamp;
    private Timer mFlingTimer;
    private Axis mX, mY;
    private float mInitialZoomSpan;
    /* The span at the first zoom event (in unzoomed page coordinates). */
    private IntPoint mInitialZoomFocus;
    /* The zoom focus at the first zoom event (in unzoomed page coordinates). */
    private boolean mTracking, mZooming;

    public PanZoomController(LayerController controller) {
        mController = controller;
        mX = new Axis(); mY = new Axis();
        mStopped = true;

        populatePositionAndLength();
    }

    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
        case MotionEvent.ACTION_DOWN:   return onTouchStart(event);
        case MotionEvent.ACTION_MOVE:   return onTouchMove(event);
        case MotionEvent.ACTION_UP:     return onTouchEnd(event);
        default:                        return false;
        }
    }

    public void geometryChanged() {
        populatePositionAndLength();
    }

    /**
     * Returns true if this controller is fine with performing a redraw operation or false if the
     * controller would prefer that this action didn't take place. This is used to optimize the
     * sending of pan/zoom events; when this returns false, a performance-critical operation like
     * a pan or a zoom is taking place.
     */
    public boolean getRedrawHint() {
        return mStopped && !mTracking && !mZooming;
    }

    /*
     * Panning/scrolling
     */

    private boolean onTouchStart(MotionEvent event) {
        /* If there is more than one finger down, we bail out to avoid misinterpreting a zoom
         * gesture as a pan gesture. */
        if (event.getPointerCount() > 1 || mZooming) {
            mZooming = true;
            mTouchMoved = false;
            mStopped = true;
            return false;
        }

        mX.touchPos = event.getX(0); mY.touchPos = event.getY(0);
        mTouchMoved = mStopped = false;
        mTracking = true;
        // TODO: Hold timeout
        return true;
    }

    private boolean onTouchMove(MotionEvent event) {
        if (event.getPointerCount() > 1 || mZooming) {
            mZooming = true;
            mTouchMoved = false;
            mStopped = true;
            return false;
        }

        if (!mTouchMoved)
            mLastTimestamp = System.currentTimeMillis();
        mTouchMoved = true;

        // TODO: Clear hold timeout
        track(event, System.currentTimeMillis());

        if (mFlingTimer != null) {
            mFlingTimer.cancel();
            mFlingTimer = null;
        }

        return true;
    }

    private boolean onTouchEnd(MotionEvent event) {
        if (mZooming)
            mZooming = false;

        mTracking = false;
        fling(System.currentTimeMillis());
        return true;
    }

    private void track(MotionEvent event, long timestamp) {
        long timeStep = timestamp - mLastTimestamp;
        mLastTimestamp = timestamp;

        mX.velocity = mX.touchPos - event.getX(0); mY.velocity = mY.touchPos - event.getY(0);
        mX.touchPos = event.getX(0); mY.touchPos = event.getY(0);

        float absVelocity = (float)Math.sqrt(mX.velocity * mX.velocity +
                                             mY.velocity * mY.velocity);
        mStopped = absVelocity < STOPPED_THRESHOLD;

        mX.applyEdgeResistance(); mX.displace();
        mY.applyEdgeResistance(); mY.displace();
        updatePosition();
    }

    private void fling(long timestamp) {
        long timeStep = timestamp - mLastTimestamp;
        mLastTimestamp = timestamp;

        if (mStopped)
            mX.velocity = mY.velocity = 0.0f;

        mX.displace(); mY.displace();

        if (mFlingTimer != null)
            mFlingTimer.cancel();

        mX.startFling(); mY.startFling();

        mFlingTimer = new Timer();
        mFlingTimer.scheduleAtFixedRate(new TimerTask() {
            public void run() { mController.post(new FlingRunnable()); }
        }, 0, 1000L/60L);
    }

    private void updatePosition() {
        Log.e("Fennec", "moving to " + mX.viewportPos + ", " + mY.viewportPos);

        mController.scrollTo(mX.viewportPos, mY.viewportPos);
        mController.notifyLayerClientOfGeometryChange();
    }

    // Populates the viewport info and length in the axes.
    private void populatePositionAndLength() {
        IntSize pageSize = mController.getPageSize();
        IntRect visibleRect = mController.getVisibleRect();
        IntSize screenSize = mController.getScreenSize();

        Log.e("Fennec", "page size: " + pageSize + " visible rect: " + visibleRect +
              "screen size: " + screenSize);

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

            // If both X and Y axes are overscrolled, we have to wait until both axes have stopped to
            // snap back to avoid a jarring effect.
            boolean waitingToSnapX = mX.getFlingState() == Axis.FLING_STATE_WAITING_TO_SNAP;
            boolean waitingToSnapY = mY.getFlingState() == Axis.FLING_STATE_WAITING_TO_SNAP;
            if (mX.getOverscroll() != Axis.NO_OVERSCROLL &&
                    mY.getOverscroll() != Axis.NO_OVERSCROLL) {
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

            if (mX.getFlingState() == Axis.FLING_STATE_STOPPED &&
                    mY.getFlingState() == Axis.FLING_STATE_STOPPED) {
                stop();
            }
        }

        private void stop() {
            mStopped = true;
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
        public static final int FLING_STATE_STOPPED = 0;            // Stopped.
        public static final int FLING_STATE_SCROLLING = 1;          // Scrolling.
        public static final int FLING_STATE_WAITING_TO_SNAP = 2;    // Waiting to snap into place.
        public static final int FLING_STATE_SNAPPING = 3;           // Snapping into place.

        public static final int OVERSCROLLED_MINUS = -1;
        /* Overscrolled in the negative direction. */
        public static final int NO_OVERSCROLL = 0;
        /* Not overscrolled. */
        public static final int OVERSCROLLED_PLUS = 1;
        /* Overscrolled in the positive direction. */

        public float touchPos;                  /* Position of the last touch. */
        public float velocity;                  /* Velocity in this direction. */

        private int mFlingState;                /* The fling state we're in on this axis. */
        private EaseOutAnimation mSnapAnim;     /* The animation when the page is snapping back. */

        /* These three need to be kept in sync with the layer controller. */
        public int viewportPos;
        private int mViewportLength;
        private int mScreenLength;
        private int mPageLength;

        public int getFlingState() { return mFlingState; }

        public void setViewportLength(int viewportLength) { mViewportLength = viewportLength; }
        public void setScreenLength(int screenLength) { mScreenLength = screenLength; }
        public void setPageLength(int pageLength) { mPageLength = pageLength; }

        private int getViewportEnd() { return viewportPos + mViewportLength; }

        public int getOverscroll() {
            if (viewportPos < 0)
                return OVERSCROLLED_MINUS;
            if (viewportPos > mPageLength - mViewportLength)
                return OVERSCROLLED_PLUS;
            return NO_OVERSCROLL;
        }

        // Returns the amount that the page has been overscrolled. If the page hasn't been
        // overscrolled on this axis, returns 0.
        private int getExcess() {
            switch (getOverscroll()) {
            case OVERSCROLLED_MINUS:    return -viewportPos;
            case OVERSCROLLED_PLUS:     return getViewportEnd() - mPageLength;
            default:                    return 0;
            }
        }

        // Applies resistance along the edges when tracking.
        private void applyEdgeResistance() {
            int excess = getExcess();
            if (excess > 0)
                velocity *= SNAP_LIMIT - (float)excess / mViewportLength;
        }

        public void startFling() { mFlingState = FLING_STATE_SCROLLING; }

        // Advances a fling animation by one step.
        public void advanceFling() {
            switch (mFlingState) {
            case FLING_STATE_SCROLLING:
                scroll();
                return;
            case FLING_STATE_WAITING_TO_SNAP:
                // We don't do anything until the controller switches us into the snapping state.
                return;
            case FLING_STATE_SNAPPING:
                snap();
                return;
            }
        }

        // Performs one frame of a scroll operation if applicable.
        private void scroll() {
            // If we aren't overscrolled, just apply friction.
            int overscroll = getOverscroll();
            if (overscroll == NO_OVERSCROLL) {
                velocity *= FRICTION;
                if (Math.abs(velocity) < 0.1f) {
                    velocity = 0.0f;
                    mFlingState = FLING_STATE_STOPPED;
                }
                return;
            }

            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - getExcess() / (mViewportLength * SNAP_LIMIT);
            if (overscroll == OVERSCROLLED_MINUS)
                velocity = Math.min((velocity + OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            else
                velocity = Math.max((velocity - OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);

            if (velocity == 0.0f)
                mFlingState = FLING_STATE_WAITING_TO_SNAP;
        }

        // Starts a snap-into-place operation.
        private void startSnap() {
            switch (getOverscroll()) {
            case OVERSCROLLED_MINUS:
                mSnapAnim = new EaseOutAnimation(viewportPos, 0.0f);
                break;
            case OVERSCROLLED_PLUS:
                mSnapAnim = new EaseOutAnimation(viewportPos, mPageLength - mViewportLength);
                break;
            default:
                throw new RuntimeException("Not overscrolled at startSnap()");
            }

            mFlingState = FLING_STATE_SNAPPING;
        }

        // Performs one frame of a snap-into-place operation.
        private void snap() {
            mSnapAnim.advance();
            viewportPos = (int)Math.round(mSnapAnim.getPosition());

            if (mSnapAnim.getFinished()) {
                mSnapAnim = null;
                mFlingState = FLING_STATE_STOPPED;
            }
        }

        // Performs displacement of the viewport position according to the current velocity.
        private void displace() { viewportPos += velocity; }
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
    public boolean onScale(ScaleGestureDetector detector) {
        float newZoom = detector.getCurrentSpan() / mInitialZoomSpan;

        IntSize screenSize = mController.getScreenSize();
        float x = mInitialZoomFocus.x - (detector.getFocusX() / newZoom);
        float y = mInitialZoomFocus.y - (detector.getFocusY() / newZoom);
        float width = screenSize.width / newZoom;
        float height = screenSize.height / newZoom;
        mController.setVisibleRect((int)Math.round(x), (int)Math.round(y),
                                   (int)Math.round(width), (int)Math.round(height));
        mController.notifyLayerClientOfGeometryChange();
        return true;
    }

    public boolean onScaleBegin(ScaleGestureDetector detector) {
        IntRect initialZoomRect = (IntRect)mController.getVisibleRect().clone();
        float initialZoom = mController.getZoomFactor();

        mInitialZoomFocus = new IntPoint((int)Math.round(initialZoomRect.x + (detector.getFocusX() / initialZoom)),
                                         (int)Math.round(initialZoomRect.y + (detector.getFocusY() / initialZoom)));
        mInitialZoomSpan = detector.getCurrentSpan() / initialZoom;
        return true;
    }

    public void onScaleEnd(ScaleGestureDetector detector) {
        // TODO
    }
}

