package netscape.javascript;

/**
 * Runs a JavaScript object with a run() method in a separate thread.
 */
public class JSRunnable implements Runnable {
	private JSObject runnable;

	public JSRunnable(JSObject runnable) {
		this.runnable = runnable;
		synchronized(this) {
			new Thread(this).start();
			try {
				this.wait();
			} catch (InterruptedException ie) {
			}
		}
	}
	
	public void run() {
		try {
			runnable.call("run", null);
			synchronized(this) {
				notifyAll();
			}
		} catch (Throwable t) {
			System.err.println(t);
			t.printStackTrace(System.err);
		}
	}
}
