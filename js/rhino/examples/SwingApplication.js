/*
 * SwingApplication.js - a translation into JavaScript of
 * SwingApplication.java, a java.sun.com Swing example.
 *
 * @author Roger E Critchlow, Jr.
 */

var swingNames = JavaImporter();

swingNames.importPackage(Packages.javax.swing);
swingNames.importPackage(Packages.java.awt);
swingNames.importPackage(Packages.java.awt.event);

function createComponents() 
{
    with (swingNames) {
        var labelPrefix = "Number of button clicks: ";
        var numClicks = 0;
        var label = new JLabel(labelPrefix + numClicks);
        var button = new JButton("I'm a Swing button!");
        button.mnemonic = KeyEvent.VK_I;
        // Since Rhino 1.5R5 JS functions can be passed to Java method if
        // corresponding argument type is Java interface with single method
        // or all its methods have the same number of arguments and the
        // corresponding arguments has the same type. See also comments for
        // frame.addWindowListener bellow
        button.addActionListener(function() {
            numClicks += 1;
            label.setText(labelPrefix + numClicks);
        });
        label.setLabelFor(button);

        /*
         * An easy way to put space between a top-level container
         * and its contents is to put the contents in a JPanel
         * that has an "empty" border.
         */
        var pane = new JPanel();
        pane.border = BorderFactory.createEmptyBorder(30, //top
                                                      30, //left
                                                      10, //bottom
                                                      30); //right
        pane.setLayout(new GridLayout(0, 1));
        pane.add(button);
        pane.add(label);

        return pane;
    }
}

with (swingNames) {
    try {
	UIManager.
            setLookAndFeel(UIManager.getCrossPlatformLookAndFeelClassName());
    } catch (e) { }

    //Create the top-level container and add contents to it.
    var frame = new swingNames.JFrame("SwingApplication");
    frame.getContentPane().add(createComponents(), BorderLayout.CENTER);

    // Pass JS function as implementation of WindowListener. It is allowed since 
    // all methods in WindowListener have the same signature. To distinguish 
    // between methods Rhino passes to JS function the name of corresponding 
    // method as the last argument  
    frame.addWindowListener(function(event, methodName) {
	if (methodName == "windowClosing") {     
            java.lang.System.exit(0);
	}
    });

    //Finish setting up the frame, and show it.
    frame.pack();
    frame.setVisible(true);
}



