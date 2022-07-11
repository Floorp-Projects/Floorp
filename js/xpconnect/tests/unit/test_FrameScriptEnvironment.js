let ppmm = Services.ppmm.getChildAt(0);

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(async function test_bindings() {
    let {strict, bound} = await new Promise(function(resolve) {
        // Use a listener to get results from child
        ppmm.addMessageListener("results", function listener(msg) {
            ppmm.removeMessageListener("results", listener);
            resolve(msg.data);
        });

        // Bind vars in first process script
        ppmm.loadProcessScript("resource://test/environment_script.js", false);

        // Check visibility in second process script
        ppmm.loadProcessScript(`data:,
            let strict = (function() { return this; })() === undefined;
            var bound = "";

            try { void vu; bound += "vu,"; } catch (e) {}
            try { void vq; bound += "vq,"; } catch (e) {}
            try { void vl; bound += "vl,"; } catch (e) {}
            try { void gt; bound += "gt,"; } catch (e) {}
            try { void ed; bound += "ed,"; } catch (e) {}
            try { void ei; bound += "ei,"; } catch (e) {}
            try { void fo; bound += "fo,"; } catch (e) {}
            try { void fi; bound += "fi,"; } catch (e) {}
            try { void fd; bound += "fd,"; } catch (e) {}

            sendAsyncMessage("results", { strict, bound });
            `, false);
    });

    // FrameScript loader should share |this| access
    if (strict) {
        if (bound != "gt,ed,ei,fo,")
            throw new Error("Unexpected global binding set - " + bound);
    } else {
        if (bound != "gt,ed,ei,fo,fi,fd,")
            throw new Error("Unexpected global binding set - " + bound);
    }
});
