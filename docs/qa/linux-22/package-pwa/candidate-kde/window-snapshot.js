const windows = workspace.windowList().map(w => ({
    internalId:String(w.internalId), caption:w.caption,
    resourceClass:w.resourceClass, resourceName:w.resourceName,
    desktopFileName:w.desktopFileName, pid:w.pid,
    iconString:String(w.icon), iconName:w.icon && w.icon.name,
    clientGeometry:{x:w.clientGeometry.x,y:w.clientGeometry.y,width:w.clientGeometry.width,height:w.clientGeometry.height},
    normalWindow:w.normalWindow, active:w.active
}));
print('QA_KAN49_CANDIDATE_CONTEXT=' + JSON.stringify({currentDesktop:String(workspace.currentDesktop.id),currentActivity:workspace.currentActivity}));
print('QA_KAN49_CANDIDATE_WINDOWS=' + JSON.stringify(windows));
