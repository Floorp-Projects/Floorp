/*
 * Timer.java
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Knife.
 *
 * The Initial Developer of the Original Code is dog.
 * Portions created by dog are
 * Copyright (C) 1998 dog <dog@dog.net.uk>. All
 * Rights Reserved.
 *
 * Contributor(s): n/a.
 *
 * You may retrieve the latest version of this package at the knife home page,
 * located at http://www.dog.net.uk/knife/
 */

package dog.util;

/**
 * A timer class that wakes up listeners after a specified number of milliseconds.
 *
 * @author dog@dog.net.uk
 * @version 0.3
 */
public final class Timer extends Thread {

	long time;
	long interval = 0;
	TimerListener listener;

	/**
	 * Constructs a timer.
	 */
	public Timer(TimerListener listener) {
		this(listener, 0, false);
	}
	
	/**
	 * Constructs a timer with the specified interval, and starts it.
	 */
	public Timer(TimerListener listener, long interval) {
		this(listener, interval, true);
	}
	
	/**
	 * Constructs a timer with the specified interval, indicating whether or not to start it.
	 */
	public Timer(TimerListener listener, long interval, boolean start) {
		this.listener = listener;
		this.interval = interval;
		time = System.currentTimeMillis();
		setDaemon(true);
		setPriority(Thread.MIN_PRIORITY);
		if (start)
			start();
	}

	// -- Accessor methods --

	/**
	 * Returns this timer's interval.
	 */
	public long getInterval() {
		return interval;
	}

	/**
	 * Sets this timer's interval.
	 */
	public void setInterval(long interval) {
		this.interval = interval;
	}

	/**
	 * Runs this timer.
	 */
	public void run() {
		boolean interrupt = false;
		while (!interrupt) {
			synchronized (this) {
				try {
					wait(interval);
				} catch (InterruptedException e) {
					interrupt = true;
				}
				listener.timerFired(new TimerEvent(this, interval));
			}
		}
	}

}