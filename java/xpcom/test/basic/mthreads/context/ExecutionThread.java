/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 */

/*
 * ExecutionThread.java
 */
import org.mozilla.xpcom.*;
import java.util.Date;


public class ExecutionThread extends Thread {
    public static int MAX_TIMEOUT = 20000;
    int timeout = Integer.MAX_VALUE;
    public static long ID = 0;
    iMThreadComponent component;

    public ExecutionThread(iMThreadComponent component){
        this.ID = new Date().getTime();
        this.setName(Long.toHexString(ID));
        
        this.component = component;
    }
    public ExecutionThread(iMThreadComponent component, int timeout){
        this.ID = new Date().getTime();
        this.setName(Long.toHexString(ID));
        this.component = component;
        if(timeout < MAX_TIMEOUT) {
            this.timeout = timeout;
        }else {
            this.timeout = MAX_TIMEOUT;
        }
    }
    public int timeoutGen() {
        return (int)Math.round(Math.random()*MAX_TIMEOUT);
    }
    public void run(){ 
        try {
            if(timeout == Integer.MAX_VALUE) {
                timeout = timeoutGen();
            }
            System.out.println("ExecutionThread(" + getName() + ") sleep to " + timeout); 
            sleep(timeout);
            component.execute(getName());
        } catch (Exception e) {
            System.out.println("Execution thread had been interrupted");
        }
    }

}
