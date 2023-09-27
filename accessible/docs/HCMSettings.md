# HCM Settings

Several Firefox settings work together to determine how web content and browser chrome are rendered. They can be hard to keep track of! Use the flowcharts below for quick reference.

## Settings that control color usage in browser chrome
- OS HCM:
	- Windows: High Contrast Mode in OS accessibility settings
	- macOS: Increase Contrast in OS accessibility settings
	- Linux: High Contrast Theme in OS accessibility settings
- FF Theme (AKA FF Colorway)
Note: OS HCM settings will only trigger HCM color usage in chrome if a user's FF theme is set to "system auto". If they have a pre-selected colorway or other FF theme (including explicit "Dark" or "Light") they will not see color changes upon enabling OS HCM.

```{mermaid}
flowchart TD
A[Is OS HCM enabeld?]
A -->|Yes| B[Is FF's theme set to System Auto?]
B --> |Yes| C[Use OS HCM colors to render browser chrome]
B -->|No| D[Use FF theme colors to render browser chrome]
A -->|No| D
```

## Settings that control color usage in content
- Colors Dialog (about:preferences > Manage Colors)
	- Dropdown with options: Always, Only with High Contrast Themes, and Never
	- Use System Colors checkbox
	- Text, Background, Visited and Unvisited Link color inputs
- Extensions like Dark Reader, or changes to user.css, may override author specified colors independent of HCM.

```{mermaid}
flowchart TD
A[What is the value of the dropdown in the colors dialog?]
A -->|Always|C

A -->|Only with High Contrast Themes| B[Is a OS HCM enabled?]
B -->|Yes| C[Is the Use System Colors checkbox checked?]
C -->|Yes, and OS HCM is on| D[Use OS HCM colors to render web content]
C -->|Yes, and OS HCM is off| D2[Use OS dark/light colors to render web content]
C-->|No| E[Use colors dialog colors to render web content]

B -->|No| F[Is a color-modifying web extension or color-modifying user.css change active?]
F -->|Yes| G[Use web extension/user.css provided colors to render web content]
F -->|No| H[Use author-provided colors to render web content]

A -->|Never|F
```
